#pragma once
#include <map>
#include <memory>
#include <string>

#include <Rendering/Essentials/Shader.h>
#include <Rendering/Essentials/Texture.h>

#include "../ECS/Registry.h"

struct lua_State;

namespace NEXUS_RESOURCES {

	class AssetManager
	{
	private:
		std::map<std::string, std::shared_ptr<NEXUS_RENDERING::Texture>> m_mapTextures{};
		std::map<std::string, std::shared_ptr<NEXUS_RENDERING::Shader>> m_mapShader{};
	public:
		AssetManager() = default;
		~AssetManager() = default;

		/*
		* @brief 检查纹理是否已存在，若不存在则创建并加载进资产管理器。
		* @param textureName 用作键的纹理名称（std::string）。
		* @param texturePath 要加载的纹理文件路径（std::string）。
		* @param pixelArt 是否为像素风：控制 Min/Mag 过滤方式。
		* @return 纹理创建并加载成功返回 true，否则返回 false。
		*/
		bool AddTexture(const std::string& textureName, const std::string& texturePath, bool pixelArt = true);

		/*
		* @brief 根据名称查找纹理并返回。
		* @param textureName 要查找的纹理名称（std::string）。
		* @return 若纹理存在返回目标纹理，否则返回一个空的纹理对象。
		*/
		const NEXUS_RENDERING::Texture& GetTexture(const std::string& textureName);

		/*
		* @brief 检查着色器是否已存在，若不存在则创建并加载进资产管理器。
		* @param shaderName 用作键的着色器名称（std::string）。
		* @param vertexPath 要加载的顶点着色器文件路径（std::string）。
		* @param fragmentPath 要加载的片元着色器文件路径（std::string）。
		* @return 着色器创建并加载成功返回 true，否则返回 false。
		*/
		bool AddShader(const std::string& shaderName, const std::string& vertexPath, const std::string& fragmentPath);

		/*
		* @brief 根据名称查找着色器并返回。
		* @param shaderName 要查找的着色器名称（std::string）。
		* @return 若着色器存在返回目标着色器，否则返回一个空的着色器对象。
		*/
		NEXUS_RENDERING::Shader& GetShader(const std::string& shaderName);

		static void CreateLuaAssetManager(lua_State* lua, NEXUS_CORE::ECS::Registry& registry);
	};
}
