#include "pch.h"
#include "Core/Profiler.h"
#include "Core/Logger.h"

#include <algorithm>
#include <fstream>
#include <vector>

namespace LV3
{
	namespace   // interne au .cpp : aucun symbole exporte
	{
		std::vector<ProfFrame> s_frames;      // reserve UNE fois dans Begin
		ProfFrame  s_cur;
		uint32_t   s_warmup = 0;
		uint32_t   s_index = 0;
		bool       s_running = false;
		bool       s_full = false;

		uint64_t   s_overheadNs = 0;
		uint64_t   s_resolutionNs = 0;
		std::chrono::steady_clock::time_point s_t0;

		// Deux grandeurs DISTINCTES, mesurees sur la machine reelle :
		//  - overhead   : cout d'une paire de bornes (ce que l'instrument AJOUTE)
		//  - resolution : plus petit ecart non nul observable (en dessous,
		//                 l'instrument ne voit RIEN ; 100 ns avec QPC a 10 MHz)
		// Unite (ns) et resolution (100 ns) ne sont pas la meme chose.
		uint64_t MeasureOverhead(uint64_t& outResolutionNs)
		{
			constexpr int kN = 10000;
			volatile uint64_t sink = 0;   // interdit a l'optimiseur de supprimer la paire
			uint64_t minNonZero = 0;

			const auto a = std::chrono::steady_clock::now();
			for (int i = 0; i < kN; ++i)
			{
				const auto x = std::chrono::steady_clock::now();
				const auto y = std::chrono::steady_clock::now();
				const uint64_t d = static_cast<uint64_t>(
					std::chrono::duration_cast<std::chrono::nanoseconds>(y - x).count());
				if (d != 0 && (minNonZero == 0 || d < minNonZero)) minNonZero = d;
				sink = sink + d;
			}
			const auto b = std::chrono::steady_clock::now();
			(void)sink;

			outResolutionNs = minNonZero;
			return static_cast<uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count() / kN);
		}

		// Quantile par nth_element : O(n), et surtout execute APRES la
		// campagne -- jamais un tri pendant la mesure.
		uint64_t Quantile(std::vector<uint64_t>& v, double q)
		{
			if (v.empty()) return 0;
			const size_t last = v.size() - 1;
			size_t k = static_cast<size_t>(q * static_cast<double>(last) + 0.5);
			if (k > last) k = last;
			std::nth_element(v.begin(), v.begin() + static_cast<ptrdiff_t>(k), v.end());
			return v[k];
		}
	}

	void Profiler::Begin(size_t expectedFrames, uint32_t warmupFrames)
	{
		LV3_ASSERT(expectedFrames > 0);
		LV3_ASSERT(warmupFrames < expectedFrames);   // sinon le resume est vide

		s_frames.clear();
		s_frames.reserve(expectedFrames);
		s_warmup = warmupFrames;
		s_index = 0;
		s_running = false;
		s_full = false;
		s_overheadNs = MeasureOverhead(s_resolutionNs);

		Logger::info("[Prof] harnais arme : " + std::to_string(expectedFrames)
			+ " frames reservees, " + std::to_string(warmupFrames)
			+ " de chauffe, cout d'une paire de bornes ~"
			+ std::to_string(s_overheadNs) + " ns, resolution "
			+ std::to_string(s_resolutionNs) + " ns");
	}

	void Profiler::BeginFrame(uint64_t simTimeMilliDays)
	{
		s_cur = ProfFrame{};                       // POD : remise a zero triviale
		s_cur.index = s_index;
		s_cur.warmup = (s_index < s_warmup) ? 1u : 0u;
		s_cur.simTimeMilliDays = simTimeMilliDays;
		s_t0 = std::chrono::steady_clock::now();
		s_running = true;
	}

	void Profiler::EndFrame()
	{
		if (!s_running) return;                    // EndFrame sans BeginFrame : on ignore
		s_running = false;

		const auto t1 = std::chrono::steady_clock::now();
		s_cur.frameNs = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - s_t0).count());
		++s_index;

		// Budget epuise : on ARRETE d'enregistrer. Une reallocation ici
		// produirait une frame aberrante qu'on lirait ensuite comme un pic
		// du moteur -- l'instrument mentirait sur ce qu'il mesure.
		if (s_frames.size() == s_frames.capacity())
		{
			if (!s_full)
			{
				s_full = true;
				Logger::warn("[Prof] budget de frames epuise ("
					+ std::to_string(s_frames.size())
					+ ") — enregistrement arrete, la simulation continue");
			}
			return;
		}
		s_frames.push_back(s_cur);
	}

	void Profiler::AddNs(EProfZone z, uint64_t ns) noexcept
	{
		if (!s_running) return;
		s_cur.ns[static_cast<size_t>(z)] += ns;    // indexation directe, zero indirection
	}

	void Profiler::Add(EProfCounter c, uint64_t n) noexcept
	{
		if (!s_running) return;
		s_cur.counters[static_cast<size_t>(c)] += n;
	}

	uint64_t Profiler::OverheadNs() noexcept { return s_overheadNs; }

	// Fige la valeur vue par CETTE unite de compilation, dans la LIB.
	int Profiler::CompiledProfileFlag() noexcept { return LV3_PROFILE; }
	size_t   Profiler::FrameCount() noexcept { return s_frames.size(); }
	bool     Profiler::IsFull()     noexcept { return s_full; }

	bool Profiler::DumpCsv(const std::string& path, const ProfRunInfo& info)
	{
		if (s_frames.empty())
		{
			Logger::warn("[Prof] aucune frame enregistree — rien a ecrire");
			return false;
		}

		// ── 1. La serie brute ────────────────────────────────────────
		// Separateur ';' et TOUTES les valeurs en ENTIER : un '.' decimal
		// dans un Excel francais est lu comme du texte, et une colonne de
		// temps devenue texte est une campagne a refaire.
		{
			std::ofstream f(path, std::ios::trunc);
			if (!f)
			{
				Logger::error("[Prof] ecriture impossible : " + path);
				return false;
			}

			f << "# scene=" << info.scene
				<< ";config=" << info.config
				<< ";res=" << info.width << 'x' << info.height
				<< ";views=" << info.views
				<< ";frames=" << s_frames.size()
				<< ";warmup=" << s_warmup
				<< ";overhead_ns=" << s_overheadNs
				<< ";resolution_ns=" << s_resolutionNs << '\n';

			f << "frame;warmup;sim_millidays;frame_ns";
			for (size_t z = 0; z < kProfZoneCount; ++z)    f << ';' << kProfZoneNames[z] << "_ns";
			for (size_t c = 0; c < kProfCounterCount; ++c) f << ';' << kProfCounterNames[c];
			f << '\n';

			for (const ProfFrame& fr : s_frames)
			{
				f << fr.index << ';' << fr.warmup << ';' << fr.simTimeMilliDays << ';' << fr.frameNs;
				for (size_t z = 0; z < kProfZoneCount; ++z)    f << ';' << fr.ns[z];
				for (size_t c = 0; c < kProfCounterCount; ++c) f << ';' << fr.counters[c];
				f << '\n';
			}
		}

		// ── 2. Le resume ─────────────────────────────────────────────
		// Mediane / p95 / max, JAMAIS de moyenne (et jamais de FPS moyens :
		// moyenne(1/t) != 1/moyenne(t)). Les frames de chauffe sont exclues
		// du calcul mais restent dans la serie brute : on ne supprime pas
		// une donnee, on la marque.
		const std::string sum = path.substr(0, path.find_last_of('.')) + "_summary.csv";
		{
			std::ofstream g(sum, std::ios::trunc);
			if (!g)
			{
				Logger::error("[Prof] ecriture impossible : " + sum);
				return false;
			}

			std::vector<uint64_t> tmp;
			tmp.reserve(s_frames.size());

			auto collectZone = [&](size_t z)
				{
					tmp.clear();
					for (const ProfFrame& fr : s_frames)
						if (!fr.warmup) tmp.push_back(fr.ns[z]);
				};

			tmp.clear();
			for (const ProfFrame& fr : s_frames) if (!fr.warmup) tmp.push_back(fr.frameNs);
			const size_t   measured = tmp.size();
			const uint64_t medianFrame = Quantile(tmp, 0.50);

			g << "# scene=" << info.scene << ";config=" << info.config
				<< ";res=" << info.width << 'x' << info.height
				<< ";views=" << info.views
				<< ";mesurees=" << measured << ";chauffe=" << s_warmup
				<< ";overhead_ns=" << s_overheadNs
				<< ";resolution_ns=" << s_resolutionNs << '\n';

			g << "zone;median_ns;p95_ns;max_ns;pourmille_de_la_frame\n";
			g << "FRAME;" << medianFrame << ';';
			{
				tmp.clear();
				for (const ProfFrame& fr : s_frames) if (!fr.warmup) tmp.push_back(fr.frameNs);
				g << Quantile(tmp, 0.95) << ';';
				tmp.clear();
				for (const ProfFrame& fr : s_frames) if (!fr.warmup) tmp.push_back(fr.frameNs);
				g << Quantile(tmp, 1.00) << ";1000\n";
			}

			for (size_t z = 0; z < kProfZoneCount; ++z)
			{
				collectZone(z); const uint64_t med = Quantile(tmp, 0.50);
				collectZone(z); const uint64_t p95 = Quantile(tmp, 0.95);
				collectZone(z); const uint64_t mx = Quantile(tmp, 1.00);
				// pour mille, en entier : pas de virgule, pas de piege de locale
				const uint64_t share = (medianFrame > 0) ? (med * 1000u) / medianFrame : 0u;
				g << kProfZoneNames[z] << ';' << med << ';' << p95 << ';' << mx << ';' << share << '\n';
			}

			g << "\ncompteur;median;p95;max;-\n";
			for (size_t c = 0; c < kProfCounterCount; ++c)
			{
				auto collectCnt = [&]()
					{
						tmp.clear();
						for (const ProfFrame& fr : s_frames)
							if (!fr.warmup) tmp.push_back(fr.counters[c]);
					};
				collectCnt(); const uint64_t med = Quantile(tmp, 0.50);
				collectCnt(); const uint64_t p95 = Quantile(tmp, 0.95);
				collectCnt(); const uint64_t mx = Quantile(tmp, 1.00);
				g << kProfCounterNames[c] << ';' << med << ';' << p95 << ';' << mx << ";0\n";
			}
		}

		Logger::success("[Prof] " + std::to_string(s_frames.size()) + " frames ecrites : "
			+ path + "  (+ resume : " + sum + ")");
		return true;
	}

} // namespace LV3