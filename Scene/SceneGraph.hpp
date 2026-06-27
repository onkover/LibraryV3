#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <fstream>	// fichier
#include <iostream>

#include <File/json.hpp>

#include <Maths/Vectorlib.h>
#include <Objects/RessourceManager/RessourceMeshManager.hpp>
#include "Registry.hpp"


#ifndef SCENE_GRAPH_INCLUDE
#define SCENE_GRAPH_INCLUDE

bool buildSceneGraph(
    const std::string& sceneFilePath,
    Registry& registry,
    Entity& out_activeCamera,
    RessourceMeshManager& pRM, world* pWorld, const char* directory);

#endif
