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

        extern QString workshopDecryptionKey;

        extern bool steamcmdEnabled;
        extern bool steamcmdUpdateSteamMods;

        extern bool useOpenSB;

        //
        void load();
        void verify();
        void save();

    }
}
