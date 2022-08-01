// Copyright(c) 2022 Sebastian Veith

#pragma once

#include "pch.h"

#include "config.h"
#include "log.h"

namespace utility
{
    class KeyboardInput
    {
      public:
        bool Init();
        bool GetKeyState(Cfg key, bool& isRepeat);

      private:
        bool UpdateKeyState(const std::set<int>& vkKeySet, bool& isRepeat);

        std::map<Cfg, std::set<int>> m_ShortCuts;
        std::map<std::set<int>, std::pair<bool, std::chrono::steady_clock::time_point>> m_KeyStates;
        const std::chrono::milliseconds m_KeyRepeatDelay = 300ms;
    };

    template <typename Sample>
    class Cache
    {
      public:
        Cache(XrTime tolerance, Sample fallback) : m_Tolerance(tolerance), m_Fallback(fallback){};

        void AddSample(XrTime time, Sample sample)
        {
            m_Cache.insert({time, sample});
        }

        Sample GetSample(XrTime time) const
        {
            TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider, "GetSample", TLArg(time, "Time"));

            LAYER_NAMESPACE::log::DebugLog("GetSample(%s): %u\n", typeid(Sample).name(), time);

            auto it = m_Cache.lower_bound(time);
            bool itIsEnd = m_Cache.end() == it;
            if (!itIsEnd)
            {
                if (it->first == time)
                {
                    // exact entry found
                    TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                      "GetSample_Found",
                                      TLArg(typeid(Sample).name(), "Type"),
                                      TLArg("Exact", "Match"),
                                      TLArg(it->first, "Time"));

                    LAYER_NAMESPACE::log::DebugLog("GetSample(%s): exact match found\n", typeid(Sample).name());

                    return it->second;
                }
                else if (it->first <= time + m_Tolerance)
                {
                    // succeeding entry is within tolerance
                    TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                      "GetSample_Found",
                                      TLArg(typeid(Sample).name(), "Type"),
                                      TLArg("Later", "Match"),
                                      TLArg(it->first, "Time"));
                    LAYER_NAMESPACE::log::DebugLog("GetSample(%s): later match found %u\n",
                                                   typeid(Sample).name(),
                                                   it->first);

                    return it->second;
                }
            }
            bool itIsBegin = m_Cache.begin() == it;
            if (!itIsBegin)
            {
                auto lowerIt = it;
                lowerIt--;
                if (lowerIt->first >= time - m_Tolerance)
                {
                    // preceding entry is within tolerance
                    TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                      "GetSample_Found",
                                      TLArg(typeid(Sample).name(), "Type"),
                                      TLArg("Earlier", "Match"),
                                      TLArg(lowerIt->first, "Time"));
                    LAYER_NAMESPACE::log::DebugLog("GetSample(%s): earlier match found: %u\n",
                                                   typeid(Sample).name(),
                                                   lowerIt->first);

                    return lowerIt->second;
                }
            }
            LAYER_NAMESPACE::log::ErrorLog("GetSample(%s) unable to find sample %u+-%dms\n",
                                           typeid(Sample).name(),
                                           time,
                                           m_Tolerance);
            if (!itIsEnd)
            {
                if (!itIsBegin)
                {
                    auto lowerIt = it;
                    lowerIt--;
                    // both etries are valid -> select better match
                    auto returnIt = (time - lowerIt->first < it->first - time ? lowerIt : it);
                    TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                      "GetSample_Failed",
                                      TLArg(typeid(Sample).name(), "Type"),
                                      TLArg("Estimated Both", "Match"),
                                      TLArg(it->first, "Time"));
                    LAYER_NAMESPACE::log::ErrorLog("Using best match: t = %u \n", returnIt->first);

                    return returnIt->second;
                }
                // higher entry is first in cache -> use it

                TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                  "GetSample_Found",
                                  TLArg(typeid(Sample).name(), "Type"),
                                  TLArg("Estimated Earlier", "Match"),
                                  TLArg(it->first, "Time"));
                LAYER_NAMESPACE::log::ErrorLog("Using best match: t = %u \n", it->first);
                return it->second;
            }
            if (!itIsBegin)
            {
                auto lowerIt = it;
                lowerIt--;
                // lower entry is last in cache-> use it
                LAYER_NAMESPACE::log::ErrorLog("Using best match: t = %u \n", lowerIt->first);
                TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                  "GetSample_Failed",
                                  TLArg(typeid(Sample).name(), "Type"),
                                  TLArg("Estimated Earlier", "Type"),
                                  TLArg(lowerIt->first, "Time"));
                return lowerIt->second;
            }
            // cache is emtpy -> return fallback
            LAYER_NAMESPACE::log::ErrorLog("Using fallback!!!\n", time);
            TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider, "GetSample_Failed", TLArg("Fallback", "Type"));
            return m_Fallback;
        }

        // remove outdated entries
        void CleanUp(XrTime time)
        {
            auto it = m_Cache.lower_bound(time - m_Tolerance);
            if (m_Cache.end() != it && m_Cache.begin() != it)
            {
                m_Cache.erase(m_Cache.begin(), it);
            }
        }

        bool empty()
        {
            return m_Cache.empty();
        }

      private:
        std::map<XrTime, Sample> m_Cache{};
        Sample m_Fallback;
        XrTime m_Tolerance;
    };

    class Mmf
    {
      public:
        ~Mmf();
        void SetName(const std::string& name);
        bool Open();
        bool Read(void* buffer, size_t size);
        void Close();


      private: 
        std::string m_Name;
        HANDLE m_FileHandle{INVALID_HANDLE_VALUE};
        void* m_View{nullptr};
    };

    std::string LastErrorMsg(DWORD error);

} // namespace utility