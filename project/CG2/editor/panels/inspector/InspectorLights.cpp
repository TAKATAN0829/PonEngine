#include "InspectorPanel.h"
#include "../../EditorProperties.h"
#include "settings/LightSettings.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

// Light共通項目とSpot固有の角度を分ける。向きはTransformの回転で編集する。
void InspectorPanel::DrawLightComponent(const EditableObjectView& object) {
#ifdef USE_IMGUI
	if (!LightTypes::IsLight(object.type)) {
		return;
	}
	if (!EditorProperties::Header(LightTypes::GetName(object.type),"Activeで有効・無効を切り替えます。Spot / Directionalの照射方向はTransformの回転（ローカル+Z）で指定します。影の生成には未対応です。")) {
		return;
	}
	if (!EditorProperties::Begin("LightFields")) {
		return;
	}
	EditorProperties::Color("Color",object.color);
	EditorProperties::Float("Intensity",object.lightIntensity,0.05f,0,100);
	if (object.type != WorldObjectType::DirectionalLight) {
		EditorProperties::Float("Range",object.lightRadius,0.1f,0.01f,1000);
		EditorProperties::Float("Decay",object.lightDecay,0.05f,0.01f,100);
	}
	if (object.type == WorldObjectType::SpotLight) {
		if (object.light != nullptr) {
			EditorProperties::Float("Outer Angle",&object.light->outerAngle,0.5f,1,179,"%.1f deg");
			if (object.light->innerAngle > object.light->outerAngle) {
				object.light->innerAngle = object.light->outerAngle;
			}
			EditorProperties::Float("Inner Angle",&object.light->innerAngle,0.5f,0,object.light->outerAngle,"%.1f deg");
		}
	}
	EditorProperties::End();
#else
	(void)object;
#endif
}
