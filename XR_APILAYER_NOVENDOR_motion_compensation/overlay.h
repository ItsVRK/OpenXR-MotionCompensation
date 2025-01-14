// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri, Sebastian Veith
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once
#include "pch.h"
#include "interfaces.h"

namespace graphics
{
    // Colors
    constexpr XrVector3f Red{1.f, 0.f, 0.f};
    constexpr XrVector3f DarkRed{0.25f, 0.f, 0.f};
    constexpr XrVector3f LightRed{1.f, 0.15f, 0.15f};
    constexpr XrVector3f Green{0.f, 1.f, 0.f};
    constexpr XrVector3f DarkGreen{0.f, 0.25f, 0.f};
    constexpr XrVector3f LightGreen{0.15f, 1.f, 0.15f};
    constexpr XrVector3f Blue{0.f, 0.f, 1.f};
    constexpr XrVector3f DarkBlue{0.f, 0.f, 0.25f};
    constexpr XrVector3f LightBlue{0.15f, 0.15f, 1.f};
    constexpr XrVector3f Yellow{1.f, 1.f, 0.f};
    constexpr XrVector3f DarkYellow{0.25f, 0.25f, 0.f};
    constexpr XrVector3f LightYellow{1.f, 1.f, 0.15f};
    constexpr XrVector3f Cyan{0.f, 1.f, 1.f};
    constexpr XrVector3f DarkCyan{0.f, 0.25f, 0.25f};
    constexpr XrVector3f LightCyan{0.15f, 1.f, 1.f};
    constexpr XrVector3f Magenta{1.f, 0.f, 1.f};
    constexpr XrVector3f DarkMagenta{0.25f, 0.f, 0.25f};
    constexpr XrVector3f LightMagenta{1.f, 0.15f, 1.f};

    struct SwapchainImages
    {
        std::shared_ptr<graphics::ITexture> appTexture;
        std::shared_ptr<graphics::ITexture> runtimeTexture;
    };

    struct SwapchainState
    {
        std::vector<SwapchainImages> images;
        uint32_t acquiredImageIndex{0};
        bool delayedRelease{false};
    };

    class Overlay
    {
      public:
        ~Overlay();
        void CreateSession(const XrSessionCreateInfo* createInfo, XrSession* session, const std::string& runtimeName);        void DestroySession(XrSession session);
        void CreateSwapchain(XrSession session,
                             const XrSwapchainCreateInfo* chainCreateInfo,
                             const XrSwapchainCreateInfo* createInfo,
                             XrSwapchain* swapchain,
                             bool isDepth);
        void DestroySwapchain(XrSwapchain swapchain);
        XrResult AcquireSwapchainImage(XrSwapchain swapchain,
                                       const XrSwapchainImageAcquireInfo* acquireInfo,
                                       uint32_t* index);
        XrResult ReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo);
        bool ToggleOverlay();
        void DrawOverlay(XrFrameEndInfo* chainFrameEndInfo,
                         const XrPosef& referenceTrackerPose,
                         const XrPosef& reversedManipulation,
                         bool mcActivated);
        
        bool m_Initialized{false};

      private:
        std::vector<SimpleMeshVertex> CreateMarker(bool rgb);
        std::vector<SimpleMeshVertex> CreateConeMesh(XrVector3f top,
                                                     XrVector3f side,
                                                     XrVector3f offset,
                                                     XrVector3f topColor,
                                                     XrVector3f sideColor,
                                                     XrVector3f bottomColor);
        std::vector<unsigned short> CreateIndices(size_t amount);

        bool m_OverlayActive{false};
        std::shared_ptr<graphics::IDevice> m_GraphicsDevice;
        std::map<XrSwapchain, graphics::SwapchainState> m_Swapchains;
        std::unordered_map<XrSwapchain, std::shared_ptr<ITexture>> m_OwnDepthBuffers;
        std::shared_ptr<graphics::ISimpleMesh> m_MeshRGB, m_MeshCMY;
    };
} // namespace graphics