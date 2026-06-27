// Présence obligatoire du fichier PCH (Precompiled Header) pour accélérer les temps de compilation. 
// Tous les .cpp du projet doivent commencer par #include "pch.h" et ce fichier doit être compilé une seule fois pour générer le fichier d'en-tête précompilé (LibraryV3.pch).
//
// pch.cpp — Compile une seule fois, génère LibraryV3.pch
// Propriété du fichier : Créer l'en-tête précompilé (/Yc"pch.h")


// Le PCH n'a de sens que si son contenu est stable (headers système, STL, libs externes). C'est exactement ce que fait ton pch.h actuel.La discipline à tenir :
//
// Dans pch.h → uniquement ce qui ne change jamais(<vector>, <cmath>, etc.)
// Dans les.cpp → #include "pch.h" en premier, puis uniquement les headers locaux du projet(MeshClass.h, etc.)
// Jamais → ré - inclure dans un.cpp ce qui est déjà dans pch.h

#include "pch.h"
