#include "serialization/LegacyLightMigration.h"
#include "objects/WorldObjects.h"
#include <limits>
#include <numbers>
#include <utility>

//=============================================================================================//
// 旧形式の移行


bool LegacyLightMigration::AddLegacyDefaults(std::vector<WorldObjectData>& objects, std::string& error, WorldObjectId minimumId) {
	if (minimumId >= (std::numeric_limits<WorldObjectId>::max)()-3) {
		error = "No IDs available for legacy lights.";

		return false;
	}

	if (objects.size()+3 > WorldObjects::kMaxObjects) {
		error = "Legacy lights exceed scene capacity.";

		return false;
	}

	WorldObjectId nextId = minimumId;

	for (const WorldObjectData& object : objects) {
		if (object.id >= (std::numeric_limits<WorldObjectId>::max)()-3) {
			error = "No IDs available for legacy lights.";

			return false;
		}

		if (object.id >= nextId) {
			nextId = object.id+1;
		}
	}

	std::vector<WorldObjectData> migrated = objects;

	WorldObjectData light;

	light.hasMeshRenderer = false;

	light.id = nextId++;

	light.type = WorldObjectType::DirectionalLight;

	light.name = "Directional Light (Migrated)";

	light.transform.rotate.x = std::numbers::pi_v<float>/2;

	migrated.push_back(light);

	light.id = nextId++;

	light.type = WorldObjectType::PointLight;

	light.name = "Point Light (Migrated)";

	light.transform.rotate = {};

	light.transform.translate = {0,2,0};

	migrated.push_back(light);

	light.id = nextId;

	light.type = WorldObjectType::SpotLight;

	light.name = "Spot Light (Migrated)";

	light.transform.translate = {2,1.25f,0};

	light.transform.rotate = {std::numbers::pi_v<float>/4,-std::numbers::pi_v<float>/2,0};

	light.lightIntensity = 4;

	light.lightRadius = 7;

	light.lightDecay = 2;

	light.light.outerAngle = 120; // 旧cosAngle=0.5、内角は0度。

	migrated.push_back(light);

	WorldObjects validation;

	if (!validation.ReplaceObjects(migrated,error)) {
		return false;
	}

	objects = std::move(migrated);

	return true;
}
