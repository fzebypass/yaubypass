#pragma once
#include <Includes/Includes.hpp>
#include <ctime>
#include <string>

namespace Custom
{
    namespace WaterMark
    {
        // Variable de progreso declarada fuera de la función para que se dibuje solo una vez
        static float progress = 1.0f; // Se inicializa completamente llena

        void Render()
        {
            ImDrawList* DrawList = ImGui::GetForegroundDrawList(); // Dibujar en la capa superior

            ImVec2 LogoSize = ImVec2(44, 44);
            const float Padding = 10;
            ImVec2 Size(250, 40);
            ImVec2 Pos(g_Variables.g_vGameWindowSize.x - Size.x, 10);

            // Declarar un color basado en los tres primeros valores de accent_color
            ImColor primaryColor = ImColor(accent_color[0], accent_color[1], accent_color[2]);

            // Dibujar el fondo del watermark
            DrawList->AddRectFilled(Pos, Pos + Size, ImColor(g_Col.ChildCol), 6, ImDrawFlags_RoundCornersLeft);

            // Obtener la hora actual del sistema en formato 12 horas con AM/PM
            std::time_t t = std::time(nullptr);
            std::tm tm;
            localtime_s(&tm, &t);
            char timeStr[12]; // Espacio suficiente para "HH:MM AM"
            std::strftime(timeStr, sizeof(timeStr), "%I:%M %p", &tm);

            // Posiciones del texto
            ImVec2 TextPos = ImVec2(Pos.x + Padding, Pos.y + 10);
            ImVec2 ClarityPos = TextPos;

            // Dibujar "Clarity" con el color basado en accent_color
            DrawList->AddText(ClarityPos, primaryColor, "Space");

            // Dibujar el resto del texto en blanco
            std::string restText = " Solutions   -   " + std::string(timeStr);
            ImVec2 RestTextPos = ImVec2(ClarityPos.x + ImGui::CalcTextSize("Space").x + 5, ClarityPos.y);
            DrawList->AddText(RestTextPos, ImColor(255, 255, 255), restText.c_str());

            // Posiciones de la barra de progreso
            ImVec2 ProgressBarMin = ImVec2(Pos.x, Pos.y + Size.y - 5);
            ImVec2 ProgressBarMax = ImVec2(Pos.x + (Size.x * progress), Pos.y + Size.y - 2);

            // Dibujar barra de progreso completamente llena (ya que progress = 1.0f)
            DrawList->AddRectFilled(ProgressBarMin, ProgressBarMax, primaryColor, 8);
        }


    }

    namespace ArrayList
    {
        void Render()
        {
        }
    }
}
