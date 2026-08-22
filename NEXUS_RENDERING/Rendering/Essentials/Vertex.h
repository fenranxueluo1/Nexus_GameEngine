#pragma once
#include <cstdint>
#include <glm/glm.hpp>

namespace NEXUS_RENDERING {

	struct Color
	{
		uint8_t r, g, b, a;
	};

	struct Vertex
	{
		glm::vec2 position{ 0.f }, uvs{ 0.f };
		Color color{ .r = 255, .g = 255, .b = 255, .a = 255 };

		void set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
		{
			color.r = r;
			color.g = g;
			color.b = b;
			color.a = a;
		}

		/*
		* @brief 接收一个 uint32_t 并用位运算把它解析成颜色的 rgba 值。
		* 该函数让我们可以轻松地用整数常量表示颜色。
		* @param uint32_t 目标颜色 -- 0xFF0000FF 表示红色
		*/
		void set_color(uint32_t newColor)
		{
			color.r = (newColor >> 24) & 0xFF;
			color.g = (newColor >> 16) & 0xFF;
			color.b = (newColor >> 8) & 0xFF;
			color.a = (newColor >> 0) & 0xFF;
		}
	};
}