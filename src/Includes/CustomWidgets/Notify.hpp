#pragma once
#include <Includes/Includes.hpp>
#include <Includes/Utils.hpp>
#include <chrono>
#include <cmath>
#include <map>

namespace NotifyManager
{
    enum eType {
        None,
        Info,
        Warning
    };

    enum eState {
        In,
        Current,
        Out,
        Expired
    };

    class NotifyClass {
    private:
        std::string Title;
        std::string Description;
        time_t ExpireTime = 0;
        time_t CreationTime = 0;
        eType Type = eType::None;
        eState CurrentState = eState::In;
    public:
        void SetTitle(std::string NewTitle) { this->Title = NewTitle; }
        void SetDescription(std::string NewDesc) { this->Description = NewDesc; }
        void SetType(eType NewType) { this->Type = NewType; }
        void SetState(eState NewState) { this->CurrentState = NewState; }
        void SetCreationTime(time_t NewCreationTime) { this->CreationTime = NewCreationTime; }
    public:
        std::string GetTitle() { return Title; }
        std::string GetDescription() { return Description; }
        time_t GetExpireTime() { return ExpireTime; }
        time_t GetCreationTime() { return CreationTime; }
        eType GetType() { return Type; }
        eState GetCurrentState() { return CurrentState; }
    public:
        time_t GetNowTime() {
            using namespace std::chrono;
            return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        }

        time_t GetTimeDiff() {
            return (time_t)(GetNowTime() - CreationTime);
        }

        NotifyClass(eType Type, time_t ExpireTime = 4000)
        {
            this->Type = Type;
            this->ExpireTime = ExpireTime;
            this->CreationTime = GetNowTime();
        }
    };

    inline std::vector<NotifyClass> NotifyList;

    inline float AnimateToward(float Current, float Target, float Speed, float Delta)
    {
        const float Factor = 1.0f - std::exp(-Speed * Delta);
        return ImLerp(Current, Target, Factor);
    }

    inline float EaseOutCubic(float Value)
    {
        const float Clamped = ImClamp(Value, 0.0f, 1.0f);
        const float Inverse = 1.0f - Clamped;
        return 1.0f - (Inverse * Inverse * Inverse);
    }

    inline void DeleteNotify(int Index)
    {
        NotifyList.erase(NotifyList.begin() + Index);
    }

    inline void Render()
    {
        const auto DrawList = ImGui::GetForegroundDrawList();
        const auto WindowSize = g_Variables.g_vGameWindowSize;
        const float Delta = ImGui::GetIO().DeltaTime;
        float NextHeight = 20.f;

        struct NotifyWid_t {
            float SlideX = 0.f;
            float YPos = -24.f;
            float Alpha = 0.f;
            float Scale = 0.985f;
        };

        static std::map<std::string, NotifyWid_t> anim;

        for (int i = 0; i < NotifyList.size(); i++)
        {
            auto& Notify = NotifyList.at(i);
            const auto Id = Notify.GetDescription() + std::to_string(Notify.GetCreationTime());
            const bool IsWarning = Notify.GetType() == eType::Warning;
            const std::string HeaderText = Notify.GetTitle().empty() ? (IsWarning ? "WARNING" : "WIZARD") : Notify.GetTitle();
            ImFont* HeaderFont = g_Variables.m_FontSmaller2 ? g_Variables.m_FontSmaller2 : g_Variables.m_FontNormal;
            ImFont* BodyFont = g_Variables.m_FontSecundary ? g_Variables.m_FontSecundary : g_Variables.m_FontNormal;

            auto NotifyAnim = anim.find(Id);
            if (NotifyAnim == anim.end())
            {
                anim.insert({ Id, NotifyWid_t() });
                NotifyAnim = anim.find(Id);
            }

            if (Notify.GetCurrentState() == eState::Expired)
            {
                Notify.SetState(eState::In);
                DeleteNotify(i);
                anim.erase(Id);
                i--;
                continue;
            }

            auto TitleTxtSize = Utils::CalcTextSize(HeaderFont, HeaderFont->FontSize, HeaderText.c_str());
            auto DescTxtSize = Utils::CalcTextSize(BodyFont, BodyFont->FontSize, Notify.GetDescription().c_str());

            const float PaddingX = 16.f;
            const float PaddingY = 13.f;
            const float Spacing = 10.f;
            const float MinWidth = 248.f;
            const float MaxWidth = 324.f;
            const float ScreenPaddingRight = 28.f;

            ImVec2 NotifySize;
            NotifySize.x = DescTxtSize.x > TitleTxtSize.x ? DescTxtSize.x : TitleTxtSize.x;
            NotifySize.x = ImClamp(NotifySize.x + (PaddingX * 2.f), MinWidth, MaxWidth);
            NotifySize.y = TitleTxtSize.y + DescTxtSize.y + (PaddingY * 2.f) + 8.f;

            if (Notify.GetCurrentState() == eState::In || Notify.GetCurrentState() == eState::Current)
            {
                NotifyAnim->second.Alpha = AnimateToward(NotifyAnim->second.Alpha, 1.f, 9.f, Delta);
                NotifyAnim->second.Scale = AnimateToward(NotifyAnim->second.Scale, 1.f, 10.f, Delta);
            }

            if (NotifyAnim->second.Alpha >= 0.98f && Notify.GetCurrentState() == eState::In) {
                Notify.SetState(eState::Current);
            }

            if (Notify.GetCurrentState() == eState::Current && Notify.GetTimeDiff() > Notify.GetExpireTime()) {
                Notify.SetState(eState::Out);
            }

            if (Notify.GetCurrentState() == eState::Out)
            {
                NotifyAnim->second.Alpha = AnimateToward(NotifyAnim->second.Alpha, 0.f, 4.8f, Delta);
                NotifyAnim->second.Scale = AnimateToward(NotifyAnim->second.Scale, 0.99f, 6.f, Delta);

                if (NotifyAnim->second.Alpha <= 0.03f)
                {
                    Notify.SetState(eState::Expired);
                }
            }

            NotifyAnim->second.YPos = AnimateToward(NotifyAnim->second.YPos, NextHeight, 12.f, Delta);

            const float NotifyStartX = WindowSize.x - NotifySize.x - ScreenPaddingRight;
            const float NotifyStartY = NotifyAnim->second.YPos;
            const float ScaledWidth = NotifySize.x * NotifyAnim->second.Scale;
            const float ScaledHeight = NotifySize.y * NotifyAnim->second.Scale;
            const float ScaleOffsetX = (NotifySize.x - ScaledWidth) * 0.5f;
            const float ScaleOffsetY = (NotifySize.y - ScaledHeight) * 0.5f;

            ImRect NotifyPos(
                ImVec2(NotifyStartX + ScaleOffsetX, NotifyStartY + ScaleOffsetY),
                ImVec2(NotifyStartX + ScaleOffsetX + ScaledWidth, NotifyStartY + ScaleOffsetY + ScaledHeight)
            );

            const ImVec4 AccentColor = IsWarning
                ? ImVec4(0.86f, 0.68f, 0.39f, 0.95f * NotifyAnim->second.Alpha)
                : ImVec4(0.90f, 0.90f, 0.92f, 0.95f * NotifyAnim->second.Alpha);
            const ImVec4 TitleColor = IsWarning
                ? ImVec4(0.93f, 0.79f, 0.56f, 0.90f * NotifyAnim->second.Alpha)
                : ImVec4(0.76f, 0.76f, 0.79f, 0.86f * NotifyAnim->second.Alpha);
            const ImVec4 DescColor = ImVec4(0.91f, 0.91f, 0.93f, 0.97f * NotifyAnim->second.Alpha);
            const ImVec4 CardColor = ImVec4(0.06f, 0.06f, 0.06f, 0.98f * NotifyAnim->second.Alpha);
            const ImVec4 BorderColor = ImVec4(1.00f, 1.00f, 1.00f, 0.035f * NotifyAnim->second.Alpha);
            constexpr float NotifyRounding = 10.f;

            DrawList->AddRectFilled(
                NotifyPos.Min,
                NotifyPos.Max,
                ImGui::GetColorU32(CardColor),
                NotifyRounding
            );

            DrawList->AddRect(
                NotifyPos.Min,
                NotifyPos.Max,
                ImGui::GetColorU32(BorderColor),
                NotifyRounding,
                0,
                1.0f
            );

            const float TextStartX = NotifyPos.Min.x + PaddingX;
            const float HeaderCenterY = NotifyPos.Min.y + PaddingY + 5.f;
            const auto DotCenter = ImVec2(TextStartX + 4.f, HeaderCenterY);
            const auto TitlePos = ImVec2(TextStartX + 12.f, HeaderCenterY - (TitleTxtSize.y * 0.5f) - 2.f);
            const auto DescPos = ImVec2(TextStartX, TitlePos.y + TitleTxtSize.y + 7.f);

            DrawList->AddCircleFilled(DotCenter, 2.5f, ImGui::GetColorU32(AccentColor), 16);

            DrawList->AddText(
                HeaderFont,
                HeaderFont->FontSize,
                TitlePos,
                ImGui::GetColorU32(TitleColor),
                HeaderText.c_str()
            );

            DrawList->AddText(
                BodyFont,
                BodyFont->FontSize,
                DescPos,
                ImGui::GetColorU32(DescColor),
                Notify.GetDescription().c_str()
            );

            const float Progress = 1.0f - ImClamp((float)Notify.GetTimeDiff() / (float)Notify.GetExpireTime(), 0.0f, 1.0f);
            if (Progress > 0.0f)
            {
                const ImVec2 BarMin = ImVec2(NotifyPos.Min.x + 12.f, NotifyPos.Max.y - 6.f);
                const ImVec2 BarMax = ImVec2(NotifyPos.Max.x - 12.f, NotifyPos.Max.y - 4.f);
                const float BarWidth = (BarMax.x - BarMin.x) * Progress;

                DrawList->AddRectFilled(BarMin, BarMax, ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.04f * NotifyAnim->second.Alpha)), 3.f);
                DrawList->AddRectFilled(BarMin, ImVec2(BarMin.x + BarWidth, BarMax.y), ImGui::GetColorU32(ImVec4(AccentColor.x, AccentColor.y, AccentColor.z, 0.60f * NotifyAnim->second.Alpha)), 3.f);
            }

            NextHeight += ScaledHeight + Spacing;
        }
    }

    inline void Send(std::string Description, time_t ExpireTime)
    {
        NotifyManager::NotifyClass Notify(NotifyManager::eType::Info, ExpireTime + 1800);
        Notify.SetTitle(xorstr("WIZARD"));
        Notify.SetDescription(Description);
        NotifyList.push_back(Notify);
    }
}
