#ifndef _FILEUTILS_H_
#define _FILEUTILS_H_

#include <string>
#include <vector>

namespace File_utils
{
    // File utilities

    const bool fileExists(const std::string &p_path);

    std::string getLowercaseFileExtension(const std::string &name);

    const std::string getFileName(const std::string &p_path);

    const std::string getShortFileName(const std::string &p_path);

    const std::string getPath(const std::string &p_path);

    const std::string getCWP();
}

#endif
