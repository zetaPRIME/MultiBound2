#pragma once

#include <QString>

namespace MultiBound {
    namespace Config {

        extern QString configPath;
        extern QString instanceRoot;

        extern QString starboundPath;
        extern QString starboundRoot;
        extern QString workshopRoot;

        //
        void load();
        void save();

    }
}
