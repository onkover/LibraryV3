/*

    Gestion basique. 
    Pris sur : https://www.scratchapixel.com/lessons/3d-basic-rendering/cam-nav-controls/3d-nav-cam-nav-controls.html

*/

#define QUAT_LIB
#pragma once

#include "../Core/config.h"
#include "Vectorlib.h"
//#include "../config.h"

template<typename T>
class Quat
{
public:
	Quat() = default;

	constexpr Quat(T s, T i, T j, T k)
		: r(s), v(i, j, k) {
	}

	constexpr Quat(T s, Vec3<T> d)
		: r(s), v(d) {
	}
	

	/*
		Construit un quaternion à partir d'angles d'Euler (en DEGRÉS).

		ATTENTION : Cette implémentation suppose un ordre de rotation
		intrinsèque Y-X-Z (Yaw, Pitch, Roll).

		eulerAngles.x = Pitch (Rotation autour de l'axe X)
		eulerAngles.y = Yaw   (Rotation autour de l'axe Y)
		eulerAngles.z = Roll  (Rotation autour de l'axe Z)
	*/
	constexpr Quat(const Vec3<T>& Angles, bool eulerAngles = false)
	{
		// 1. Convertir les angles de degrés en radians ET les diviser par 2
		//    (car les formules de quaternion utilisent le demi-angle)

		Vec3f _angles = Angles;
		if (eulerAngles)
		{
			_angles.x *= LV3::TO_RADIAN;
			_angles.y *= LV3::TO_RADIAN;
			_angles.z *= LV3::TO_RADIAN;
		}
		T pitch_half = _angles.x * (T)0.5;
		T yaw_half = _angles.y * (T)0.5;
		T roll_half = _angles.z * (T)0.5;

		// 2. Calculer les sinus et cosinus de ces demi-angles
		T c_pitch = std::cos(pitch_half);
		T s_pitch = std::sin(pitch_half);
		T c_yaw = std::cos(yaw_half);
		T s_yaw = std::sin(yaw_half);
		T c_roll = std::cos(roll_half);
		T s_roll = std::sin(roll_half);

		// 3. Calculer le quaternion final en multipliant les rotations individuelles.
		// L'ordre est crucial. L'ordre Y-X-Z (Yaw, puis Pitch, puis Roll)
		// correspond à la multiplication : q_final = q_roll * q_pitch * q_yaw

		// Formule directe (évite les multiplications de quaternions intermédiaires)
		// pour l'ordre Y-X-Z
		r = c_roll * c_pitch * c_yaw + s_roll * s_pitch * s_yaw;
		v.x = c_roll * s_pitch * c_yaw + s_roll * c_pitch * s_yaw;
		v.y = c_roll * c_pitch * s_yaw - s_roll * s_pitch * c_yaw;
		v.z = s_roll * c_pitch * c_yaw - c_roll * s_pitch * s_yaw;
	}


	/*
	Définit ce quaternion à partir d'angles d'Euler (en DEGRÉS).

	ATTENTION : Suppose un ordre de rotation Y-X-Z (Yaw, Pitch, Roll).

	eulerAngles.x = Pitch (Rotation autour de l'axe X)
	eulerAngles.y = Yaw   (Rotation autour de l'axe Y)
	eulerAngles.z = Roll  (Rotation autour de l'axe Z)
	*/
	constexpr Quat<T>& SetEulerAngles(const Vec3<T>& eulerAngles) {

		T pitch_half = eulerAngles.x * LV3::TO_RADIAN * (T)0.5;
		T yaw_half = eulerAngles.y * LV3::TO_RADIAN * (T)0.5;
		T roll_half = eulerAngles.z * LV3::TO_RADIAN * (T)0.5;

		T c_pitch = std::cos(pitch_half);
		T s_pitch = std::sin(pitch_half);
		T c_yaw = std::cos(yaw_half);
		T s_yaw = std::sin(yaw_half);
		T c_roll = std::cos(roll_half);
		T s_roll = std::sin(roll_half);

		// Ordre Y-X-Z (Yaw, Pitch, Roll)
		r = c_roll * c_pitch * c_yaw + s_roll * s_pitch * s_yaw;
		v.x = c_roll * s_pitch * c_yaw + s_roll * c_pitch * s_yaw;
		v.y = c_roll * c_pitch * s_yaw - s_roll * s_pitch * c_yaw;
		v.z = s_roll * c_pitch * c_yaw - c_roll * s_pitch * s_yaw;

		return *this;
	}

	/*
		Définit un quaternion représentant une rotation spécifique dans l'espace 3D en fonction d'un axe de rotation donné (axis) et d'un angle de rotation (radians).

		axis : est un vecteur 3D (Vec3<T>) qui représente l'axe de rotation. Pour construire un quaternion à partir d'un axe et d'un angle, il est nécessaire que cet axe soit un vecteur unitaire (de longueur 1).
		r : est la partie scalaire du quaternion.
		v : est la partie vectorielle du quaternion, qui est proportionnelle à l'axe de rotation multiplié par le sinus de la moitié de l'angle de rotation.

		Pour construire le quaternion représentant une rotation autour d'un axe donné, on utilise les formules suivantes :
		v = axe * sin (radian / 2)
		r = cos (radian / 2 )

	*/
	constexpr Quat<T>& SetAxisAngle(const Vec3<T>& axis, T radians) {
		v = axis.Normalized() * std::sin(radians / 2);	// calcule la partie vectorielle du quaternion. Cela donne la direction de rotation et son amplitude en tant que partie vectorielle du quaternion.
		r = std::cos(radians / 2);						// calcule la partie scalaire du quaternion. Elle représente la "quantité" de rotation (une sorte de pondération scalaire).
		return *this;
	}

	// Empêche les erreurs de s'accumuler après de nombreuses multiplications.
	constexpr Quat<T>& normalize() {
		T mag = std::sqrt(r * r + v.x * v.x + v.y * v.y + v.z * v.z);
		if (mag > 0) {
			r /= mag;
			v /= mag;
		}
		return *this;
	}

	constexpr Matrix44<T> ToMatrix44() const
	{
		return Matrix44<T>(
			1 - 2 * (v.y * v.y + v.z * v.z), 2 * (v.x * v.y + v.z * r), 2 * (v.z * v.x - v.y * r), 0,
			2 * (v.x * v.y - v.z * r), 1 - 2 * (v.z * v.z + v.x * v.x), 2 * (v.y * v.z + v.x * r), 0,
			2 * (v.z * v.x + v.y * r), 2 * (v.y * v.z - v.x * r), 1 - 2 * (v.y * v.y + v.x * v.x), 0,
			0, 0, 0, 1);
	}

	// FONCTION CORRIGÉE : Conversion en Matrice 4x4
	 // Cette version correspond à la formule standard pour une matrice row-major.
	constexpr Matrix44<T> ToMatrix44_OLD() const {
		Quat<T> q = *this; // On fait une copie pour s'assurer qu'il est normalisé
		q.normalize();

		T x = q.v.x, y = q.v.y, z = q.v.z, w = q.r;

		return Matrix44<T>(
			1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y), 0,
			2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x), 0,
			2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y), 0,
			0, 0, 0, 1);
	}
		/*
		structure d'un quaternion : q = w + xi + yj + zk, où w est la partie scalaire et (x, y, z) la partie vectorielle.
		Les formules que vous avez citées sont dérivées de la représentation matricielle d'une rotation par quaternion. Voici l'explication pour chaque élément :

		Première ligne : (1-2y²-2z², 2xy-2zw, 2xz+2yw, 0)

		(1-2y²-2z²) : Cette formule assure que la rotation préserve la longueur du vecteur le long de l'axe x.
		(2xy-2zw) : Représente la contribution de la rotation autour de l'axe z à la nouvelle composante x.
		(2xz+2yw) : Représente la contribution de la rotation autour de l'axe y à la nouvelle composante x.


		Deuxième ligne : (2xy+2zw, 1-2x²-2z², 2yz-2xw, 0)

		(2xy+2zw) : Représente la contribution de la rotation autour de l'axe z à la nouvelle composante y.
		(1-2x²-2z²) : Cette formule assure que la rotation préserve la longueur du vecteur le long de l'axe y.
		(2yz-2xw) : Représente la contribution de la rotation autour de l'axe x à la nouvelle composante y.


		Troisième ligne : (2xz-2yw, 2yz+2xw, 1-2x²-2y², 0)

		(2xz-2yw) : Représente la contribution de la rotation autour de l'axe y à la nouvelle composante z.
		(2yz+2xw) : Représente la contribution de la rotation autour de l'axe x à la nouvelle composante z.
		(1-2x²-2y²) : Cette formule assure que la rotation préserve la longueur du vecteur le long de l'axe z.


		Quatrième ligne : (0, 0, 0, 1)

		Cette ligne est toujours (0, 0, 0, 1) pour une matrice de rotation pure en coordonnées homogènes.



		Ces formules sont dérivées de l'algèbre des quaternions et de leur application aux rotations 3D. Voici quelques points clés à comprendre :

		Les termes 1-2y²-2z², 1-2x²-2z², et 1-2x²-2y² sur la diagonale principale assurent que la matrice est orthogonale (ses colonnes sont perpendiculaires entre elles).
		Les termes hors diagonale (comme 2xy-2zw) représentent les interactions entre les différents axes de rotation.
		La symétrie de la matrice (par exemple, 2xy-2zw dans la première ligne et 2xy+2zw dans la deuxième) assure que la rotation est cohérente dans toutes les directions.
		Le facteur 2 qui apparaît dans de nombreux termes vient de la nature du produit des quaternions.
		Le w dans ces formules est équivalent au r dans notre implémentation précédente (la partie scalaire du quaternion).

		Ces formules, bien que complexes à première vue, sont optimisées pour une conversion rapide et efficace des quaternions en matrices de rotation, ce qui est crucial dans de nombreuses applications 3D en temps réel.

		| 1 - 2y² - 2z²    2xy - 2zw      2xz + 2yw      0|
		|2xy + 2zw        1 - 2x² - 2z²  2yz - 2xw      0|
		|2xz - 2yw        2yz + 2xw      1 - 2x² - 2y²  0|
		|0                0              0              1 |

		Les étapes principales :
			1. Rappel sur les quaternions : Un quaternion unitaire q = w + xi + yj + zk peut représenter une rotation 3D.
			2. Rotation d'un vecteur par un quaternion :
				Pour faire pivoter un vecteur v, on utilise l'opération : v' = qvq ^ (-1) où q ^ (-1) est le conjugué de q(pour un quaternion unitaire, c'est simplement w - xi - yj - zk)
			3. Développement de la rotation : Soit v = (x, y, z) le vecteur à faire pivoter. On peut l'écrire comme un quaternion pur : v = 0 + xi + yj + zk
				v' = qvq^(-1) = (w + ai + bj + ck)(0 + xi + yj + zk)(w - ai - bj - ck)
			4. Multiplication des quaternions : En développant cette multiplication(un processus algébrique long et complexe), on obtient une expression pour chaque composante du vecteur résultant.
			5. Regroupement des termes : Après développement et simplification, on peut regrouper les termes pour obtenir une expression de la forme :
				x' = (1 - 2b² - 2c²)x + (2ab - 2cw)y + (2ac + 2bw)z
				y' = (2ab + 2cw)x + (1 - 2a² - 2c²)y + (2bc - 2aw)z
				z' = (2ac - 2bw)x + (2bc + 2aw)y + (1 - 2a² - 2b²)z
			6. Correspondance avec la matrice de rotation : Ces expressions correspondent exactement aux lignes de notre matrice de rotation :
				| 1 - 2b² - 2c²    2ab - 2cw        2ac + 2bw    |
				|2ab + 2cw        1 - 2a² - 2c²    2bc - 2aw    |
				|2ac - 2bw        2bc + 2aw        1 - 2a² - 2b² |
				où(w, a, b, c) dans cette notation correspondent à(w, x, y, z) dans notre quaternion original.
			7. Ajout de la quatrième ligne et colonne : Pour compléter la matrice 4x4 utilisée en graphiques 3D, on ajoute simplement(0, 0, 0, 1) comme dernière ligne et colonne.

			Cette dérivation montre comment l'opération de rotation par quaternion, lorsqu'elle est exprimée sous forme matricielle, donne exactement la matrice que nous utilisons pour la conversion quaternion - matrice.
			Les avantages de cette formulation incluent :
			* Efficacité computationnelle(moins d'opérations que d'autres méthodes)
			* Stabilité numérique(moins sujette aux problèmes de précision)
			* Pas de singularités(contrairement aux angles d'Euler)

			*/
	
	T r{ 1 }; // The real part - r est la partie scalaire du quaternion.
	Vec3<T> v{ 0,0,0 }; // The imaginary vector - v est la partie vectorielle du quaternion
};

//// Quaternion multiplication
//template<typename T>
//constexpr inline Quat<T> operator* (const Quat<T>& q1, const Quat<T>& q2) {
//	return Quat<T>(
//		//q1.r * q2.r - (q1.v ^ q2.v), q1.r * q2.v + q1.v * q2.r + q1.v % q2.v);
//		q1.r* q2.r - (q1.v.dotProduct(q2.v)), q1.r* q2.v + q1.v * q2.r + q1.v.crossProduct(q2.v));
//}

typedef Quat<double> Quatd;
typedef Quat<float> Quatf;

// Quaternion multiplication
template<typename T>
constexpr inline Quat<T> operator* (const Quat<T>& q1, const Quat<T>& q2) {
	return Quat<T>(
		//q1.r * q2.r - (q1.v ^ q2.v), q1.r * q2.v + q1.v * q2.r + q1.v % q2.v);
		q1.r * q2.r - (q1.v.dotProduct(q2.v)), q1.r * q2.v + q1.v * q2.r + q1.v.crossProduct(q2.v));
}

// Produit scalaire (Dot product) pour deux quaternions
template<typename T>
constexpr inline T Dot(const Quat<T>& q1, const Quat<T>& q2) {
	// Le produit scalaire de deux quaternions q1 et q2 est :
	// q1.r*q2.r + q1.v.x*q2.v.x + q1.v.y*q2.v.y + q1.v.z*q2.v.z
	// Nous supposons que Vec3<T> a une fonction dotProduct.
	return q1.r * q2.r + q1.v.dotProduct(q2.v);
}

// Addition de deux quaternions
template<typename T>
constexpr inline Quat<T> operator+ (const Quat<T>& q1, const Quat<T>& q2) {
	// Requis pour l'interpolation linéaire (LERP)
	return Quat<T>(q1.r + q2.r, q1.v + q2.v);
}

// Multiplication par un scalaire (scalaire à gauche)
template<typename T>
constexpr inline Quat<T> operator* (T s, const Quat<T>& q) {
	// Requis pour l'interpolation
	return Quat<T>(s * q.r, s * q.v);
}

// Multiplication par un scalaire (scalaire à droite)
template<typename T>
constexpr inline Quat<T> operator* (const Quat<T>& q, T s) {
	// Requis pour l'interpolation
	return Quat<T>(q.r * s, q.v * s);
}

template<typename T>
static Quat<T> Slerp(const Quat<T>& q1, const Quat<T>& q2, T t)
{
	// Les fonctions trigonométriques comme acos/sin ne sont pas constexpr avant C++20

	// 1. Calculer le produit scalaire (cosinus de l'angle)
	T cosOmega = Dot(q1, q2);

	// Copie de q2 pour la manipulation
	Quat<T> q2_prime = q2;

	// 2. Gérer le "chemin le plus long" (Anti-podal)
	// Si cosOmega est négatif, les quaternions sont distants de > 90 degrés.
	// Inverser l'un d'eux (q2) revient à la *même rotation* mais
	// garantit que nous prenons le chemin le plus court (< 90 degrés).
	if (cosOmega < (T)0.0) {
		q2_prime.r = -q2.r;
		// Nous supposons que T * Vec3<T> est défini, car il est utilisé dans 'operator*'
		q2_prime.v = (T)-1.0 * q2.v;
		cosOmega = -cosOmega; // Le cosinus est maintenant positif
	}

	T scale1;
	T scale2;

	// 3. Gérer le cas "presque colinéaire"
	// Si les quaternions sont très proches (cosOmega ~ 1),
	// sin(omega) sera proche de 0, causant une division par zéro.
	const T LERP_THRESHOLD = (T)0.9995;
	if (cosOmega > LERP_THRESHOLD) {
		// Les angles sont trop petits. Utiliser LERP (interpolation linéaire)
		// qui est une excellente approximation pour de petits angles.
		scale1 = (T)1.0 - t;
		scale2 = t;
	}
	else {
		// 4. Cas standard : SLERP
		// Calculer l'angle Omega
		T omega = std::acos(cosOmega);
		// Dénominateur commun (sin(Omega))
		T sinOmega = std::sin(omega);

		// Calculer les coefficients d'interpolation
		scale1 = std::sin(((T)1.0 - t) * omega) / sinOmega;
		scale2 = std::sin(t * omega) / sinOmega;
	}

	// 5. Appliquer l'interpolation
	// (scale1 * q1) + (scale2 * q2_prime)
	Quat<T> result = scale1 * q1 + scale2 * q2_prime;

	// 6. Re-normaliser si nous avons utilisé LERP
	// (La formule SLERP garantit un résultat unitaire si les entrées le sont,
	// mais LERP ne le garantit pas.)
	if (cosOmega > LERP_THRESHOLD) {
		result.normalize();
	}

	return result;
}