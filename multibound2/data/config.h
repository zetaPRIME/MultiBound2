#pragma once

#include <QString>

namespace MultiBound {
    namespace Config {

        extern QString configPath;
        extern QString instanceRoot;

        extern QString starboundPath;
        extern QString starboundRoot;
        extern QString workshopRoot;
        extern QString steamcmdDLRoot;
        extern QString steamcmdWorkshopRoot;
        
        extern QString steamUsername;
        extern QString steamPassword;

        extern bool steamcmdEnabled;
        extern bool steamcmdUpdateSteamMods;

        //
        void load();
        void save();

    }
}
