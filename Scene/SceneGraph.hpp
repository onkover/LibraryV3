#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <fstream>	// fichier
#include <iostream>

#include "../ressources/json.hpp"

#include <Maths/Vectorlib.h>
#include "../Ressources/ResourceManager.h"
#include "Registry.hpp"


#ifndef SCENE_GRAPH_INCLUDE
#define SCENE_GRAPH_INCLUDE

namespace LibV3
{
    bool buildSceneGraph(
        const std::string& sceneFilePath,
        Registry& registry,
        Entity& out_activeCamera,
        ResourceManager& pRM, const char* directory);

#endif

}