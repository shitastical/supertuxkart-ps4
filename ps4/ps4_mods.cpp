// SuperTuxKart PS4: drag-and-drop add-on installer.
//
// The game itself can only download add-ons into its add-ons directory, which
// used to be an unwritable path inside /app0.  This importer makes a plain
// drop folder useful instead:
//
//   /data/stk/mods/                 <- internal storage, always available
//   /mnt/usb0/mods/                 <- or any USB drive, same layout
//   /mnt/usb0/SuperTuxKart/mods/
//
// Drop in official add-on .zip files or already-unpacked kart/track folders.
// Each zip is checked for kart.xml/track.xml before it is extracted, then the
// kart or track is copied to the normal add-ons directory.  A small state file
// under /data/stk/mods remembers what was already imported, so a USB drive can
// stay plugged in without copying the same mod on every launch.

#include "ps4_mods.hpp"

#include <string>

#include "addons/zip.hpp"
#include "graphics/irr_driver.hpp"
#include "io/file_manager.hpp"
#include "ps4_kit_diag.h"
#include "utils/log.hpp"

#include <IrrlichtDevice.h>
#include <IFileSystem.h>
#include <IFileArchive.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <exception>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <vector>

namespace
{

const char* const kInternalMods = "/data/stk/mods";
const unsigned int kMaxScanDepth = 4;

enum AddonKind
{
    ADDON_NONE,
    ADDON_KART,
    ADDON_TRACK
};

struct AddonRoot
{
    std::string path;
    AddonKind kind;
    bool use_fallback_name;
};

struct ImportContext
{
    std::map<std::string, std::string> imported;
    std::set<std::string> seen;
    bool state_dirty = false;
    int installed = 0;
    int failed = 0;
};

// ----------------------------------------------------------------------------
// Path helpers
// ----------------------------------------------------------------------------

std::string joinPath(const std::string& dir, const std::string& name)
{
    if (dir.empty())
        return name;
    if (dir[dir.size() - 1] == '/')
        return dir + name;
    return dir + "/" + name;
}   // joinPath

std::string baseName(std::string path)
{
    while (path.size() > 1 && path[path.size() - 1] == '/')
        path.erase(path.size() - 1);
    const std::string::size_type slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return path;
    return path.substr(slash + 1);
}   // baseName

std::string lowerString(const std::string& value)
{
    std::string result(value);
    for (std::string::size_type i = 0; i < result.size(); i++)
    {
        const unsigned char c = (unsigned char)result[i];
        if (c >= 'A' && c <= 'Z')
            result[i] = (char)(c - 'A' + 'a');
    }
    return result;
}   // lowerString

bool isDirectory(const std::string& path)
{
    struct stat info;
    return lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}   // isDirectory

bool isRegularFile(const std::string& path)
{
    struct stat info;
    return lstat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}   // isRegularFile

bool isZipName(const std::string& name)
{
    return name.size() >= 4 &&
           lowerString(name.substr(name.size() - 4)) == ".zip";
}   // isZipName

bool listDirectory(const std::string& path, std::vector<std::string>& entries)
{
    entries.clear();
    DIR* directory = opendir(path.c_str());
    if (!directory)
        return false;

    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL)
    {
        const std::string name(entry->d_name);
        if (name == "." || name == "..")
            continue;
        entries.push_back(name);
    }
    closedir(directory);
    std::sort(entries.begin(), entries.end());
    return true;
}   // listDirectory

bool isSkippedName(const std::string& name)
{
    return name.empty() || name[0] == '.' || name == "__MACOSX" ||
           name == "installed" || name == "Installed";
}   // isSkippedName

std::string safeDirectoryName(const std::string& name)
{
    std::string result;
    for (std::string::size_type i = 0; i < name.size(); i++)
    {
        const unsigned char c = (unsigned char)name[i];
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || c == 0 ||
            c < 0x20)
        {
            result += '_';
        }
        else
        {
            result += (char)c;
        }
    }

    while (!result.empty() && (result[0] == ' ' || result[0] == '.'))
        result.erase(0, 1);
    while (!result.empty() && (result[result.size() - 1] == ' ' ||
                               result[result.size() - 1] == '.'))
        result.erase(result.size() - 1);
    return result;
}   // safeDirectoryName

bool hasFile(const std::string& dir, const char* name)
{
    return isRegularFile(joinPath(dir, name));
}   // hasFile

AddonKind addonKindAt(const std::string& dir)
{
    if (hasFile(dir, "kart.xml"))
        return ADDON_KART;
    if (hasFile(dir, "track.xml"))
        return ADDON_TRACK;
    return ADDON_NONE;
}   // addonKindAt

// ----------------------------------------------------------------------------
// File copying and cleanup
// ----------------------------------------------------------------------------

bool removeTree(const std::string& path)
{
    struct stat info;
    if (lstat(path.c_str(), &info) != 0)
        return errno == ENOENT;

    if (S_ISDIR(info.st_mode))
    {
        std::vector<std::string> entries;
        if (!listDirectory(path, entries))
            return false;
        for (std::vector<std::string>::const_iterator it = entries.begin();
             it != entries.end(); ++it)
        {
            if (!removeTree(joinPath(path, *it)))
                return false;
        }
        return rmdir(path.c_str()) == 0;
    }

    return unlink(path.c_str()) == 0;
}   // removeTree

bool copyRegularFile(const std::string& from, const std::string& to)
{
    const int input = open(from.c_str(), O_RDONLY);
    if (input < 0)
        return false;

    const int output = open(to.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output < 0)
    {
        close(input);
        return false;
    }

    bool ok = true;
    char buffer[64 * 1024];
    for (;;)
    {
        const ssize_t count = read(input, buffer, sizeof(buffer));
        if (count == 0)
            break;
        if (count < 0)
        {
            if (errno == EINTR)
                continue;
            ok = false;
            break;
        }

        ssize_t written = 0;
        while (written < count)
        {
            const ssize_t result =
                write(output, buffer + written, (size_t)(count - written));
            if (result < 0)
            {
                if (errno == EINTR)
                    continue;
                ok = false;
                break;
            }
            written += result;
        }
        if (!ok)
            break;
    }

    close(input);
    if (close(output) != 0)
        ok = false;
    if (!ok)
        unlink(to.c_str());
    return ok;
}   // copyRegularFile

bool copyTree(const std::string& from, const std::string& to)
{
    struct stat info;
    if (lstat(from.c_str(), &info) != 0)
        return false;

    // Never follow links out of the mod, and never copy a link into the
    // add-ons directory where the game could follow it later.
    if (S_ISLNK(info.st_mode))
        return false;

    if (S_ISDIR(info.st_mode))
    {
        if (mkdir(to.c_str(), 0777) != 0 && errno != EEXIST)
            return false;

        std::vector<std::string> entries;
        if (!listDirectory(from, entries))
            return false;
        for (std::vector<std::string>::const_iterator it = entries.begin();
             it != entries.end(); ++it)
        {
            if (!copyTree(joinPath(from, *it), joinPath(to, *it)))
                return false;
        }
        return true;
    }

    if (!S_ISREG(info.st_mode))
        return true;
    return copyRegularFile(from, to);
}   // copyTree

bool publishDirectory(const std::string& fresh, const std::string& destination)
{
    const std::string old = destination + ".ps4-old";
    if (!removeTree(old))
        return false;

    const bool had_old = isDirectory(destination) || isRegularFile(destination);
    if (had_old && rename(destination.c_str(), old.c_str()) != 0)
    {
        // A stale or read-only destination is better removed than blocking an
        // update forever.  The fresh copy is still complete in this case.
        if (!removeTree(destination))
            return false;
    }

    if (rename(fresh.c_str(), destination.c_str()) != 0)
    {
        if (had_old)
            rename(old.c_str(), destination.c_str());
        removeTree(fresh);
        return false;
    }

    if (had_old)
        removeTree(old);
    return true;
}   // publishDirectory

// ----------------------------------------------------------------------------
// Source discovery
// ----------------------------------------------------------------------------

void collectAddonRoots(const std::string& dir, unsigned int depth,
                       std::vector<AddonRoot>& roots)
{
    const AddonKind kind = addonKindAt(dir);
    if (kind != ADDON_NONE)
    {
        AddonRoot root;
        root.path = dir;
        root.kind = kind;
        root.use_fallback_name = false;
        roots.push_back(root);
        return;
    }

    if (depth >= kMaxScanDepth)
        return;

    std::vector<std::string> entries;
    if (!listDirectory(dir, entries))
        return;
    for (std::vector<std::string>::const_iterator it = entries.begin();
         it != entries.end(); ++it)
    {
        if (isSkippedName(*it))
            continue;
        const std::string child = joinPath(dir, *it);
        if (!isDirectory(child))
            continue;
        collectAddonRoots(child, depth + 1, roots);
    }
}   // collectAddonRoots

// ----------------------------------------------------------------------------
// Source state
// ----------------------------------------------------------------------------

void stampTree(const std::string& path, unsigned int depth,
               unsigned long long& files, unsigned long long& bytes,
               long long& newest)
{
    if (depth > 8)
        return;

    struct stat info;
    if (lstat(path.c_str(), &info) != 0)
        return;

    if (info.st_mtime > newest)
        newest = (long long)info.st_mtime;

    if (S_ISREG(info.st_mode))
    {
        files++;
        bytes += (unsigned long long)info.st_size;
        return;
    }
    if (!S_ISDIR(info.st_mode))
        return;

    std::vector<std::string> entries;
    if (!listDirectory(path, entries))
        return;
    for (std::vector<std::string>::const_iterator it = entries.begin();
         it != entries.end(); ++it)
        stampTree(joinPath(path, *it), depth + 1, files, bytes, newest);
}   // stampTree

bool sourceStamp(const std::string& path, std::string& stamp)
{
    struct stat info;
    if (lstat(path.c_str(), &info) != 0)
        return false;

    char text[128];
    if (S_ISREG(info.st_mode))
    {
        snprintf(text, sizeof(text), "f:%llu:%lld",
                 (unsigned long long)info.st_size,
                 (long long)info.st_mtime);
        stamp = text;
        return true;
    }

    if (S_ISDIR(info.st_mode))
    {
        unsigned long long files = 0;
        unsigned long long bytes = 0;
        long long newest = 0;
        stampTree(path, 0, files, bytes, newest);
        snprintf(text, sizeof(text), "d:%llu:%llu:%lld", files, bytes, newest);
        stamp = text;
        return true;
    }

    return false;
}   // sourceStamp

std::string stateFile()
{
    return joinPath(kInternalMods, ".ps4-imported");
}   // stateFile

void loadState(ImportContext& context)
{
    std::ifstream input(stateFile().c_str());
    if (!input.is_open())
        return;

    std::string line;
    while (std::getline(input, line))
    {
        const std::string::size_type tab = line.find('\t');
        if (tab == std::string::npos || tab == 0)
            continue;
        context.imported[line.substr(0, tab)] = line.substr(tab + 1);
    }
}   // loadState

void saveState(ImportContext& context)
{
    const std::string path = stateFile();
    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary.c_str(), std::ios::out | std::ios::trunc);
    if (!output.is_open())
    {
        Log::warn("ps4mods", "Could not write mod state file '%s'.",
                  temporary.c_str());
        return;
    }

    for (std::map<std::string, std::string>::const_iterator it =
             context.imported.begin(); it != context.imported.end(); ++it)
        output << it->first << '\t' << it->second << '\n';
    output.close();

    if (rename(temporary.c_str(), path.c_str()) != 0)
    {
        Log::warn("ps4mods", "Could not replace mod state file '%s'.",
                  path.c_str());
        unlink(temporary.c_str());
    }
}   // saveState

bool alreadyImported(const std::string& source, const std::string& stamp,
                     ImportContext& context)
{
    std::map<std::string, std::string>::const_iterator it =
        context.imported.find(source);
    if (it == context.imported.end() || it->second != stamp)
        return false;
    Log::info("ps4mods", "Already imported '%s', skipping.", source.c_str());
    return true;
}   // alreadyImported

// ----------------------------------------------------------------------------
// Installation
// ----------------------------------------------------------------------------

bool installRoot(const AddonRoot& root, const std::string& fallback_name,
                 ImportContext& context)
{
    std::string name;
    if (!root.use_fallback_name)
        name = safeDirectoryName(baseName(root.path));
    if (name.empty())
        name = safeDirectoryName(fallback_name);
    if (name.empty())
        name = "mod";

    const char* const type_dir =
        root.kind == ADDON_KART ? "karts/" : "tracks/";
    if (!file_manager->checkAndCreateDirectoryP(
            file_manager->getAddonsFile(type_dir)))
    {
        Log::error("ps4mods", "Could not create the add-ons %s directory.",
                   type_dir);
        context.failed++;
        return false;
    }

    const std::string destination =
        file_manager->getAddonsFile(type_dir + name);
    const std::string fresh = destination + ".ps4-new";

    if (!removeTree(fresh))
    {
        Log::error("ps4mods", "Could not clear staging directory '%s'.",
                   fresh.c_str());
        context.failed++;
        return false;
    }

    if (!copyTree(root.path, fresh))
    {
        Log::error("ps4mods", "Could not copy '%s' to '%s'.",
                   root.path.c_str(), fresh.c_str());
        removeTree(fresh);
        context.failed++;
        return false;
    }

    if (!publishDirectory(fresh, destination))
    {
        Log::error("ps4mods", "Could not install '%s' at '%s'.",
                   root.path.c_str(), destination.c_str());
        removeTree(fresh);
        context.failed++;
        return false;
    }

    Log::info("ps4mods", "Installed %s '%s' -> '%s'.",
              root.kind == ADDON_KART ? "kart" : "track",
              root.path.c_str(), destination.c_str());
    ps4_boot_tracef("port: mods: installed %s at %s",
                    root.kind == ADDON_KART ? "kart" : "track",
                    destination.c_str());
    context.installed++;
    return true;
}   // installRoot

bool importRoots(const std::string& source,
                 const std::vector<AddonRoot>& roots,
                 const std::string& fallback_name,
                 ImportContext& context)
{
    if (roots.empty())
    {
        Log::info("ps4mods", "No kart.xml or track.xml found in '%s'.",
                  source.c_str());
        return false;
    }

    int installed = 0;
    int failed = 0;
    for (std::vector<AddonRoot>::const_iterator it = roots.begin();
         it != roots.end(); ++it)
    {
        if (installRoot(*it, fallback_name, context))
            installed++;
        else
            failed++;
    }

    if (installed > 0 && failed == 0)
        return true;
    Log::warn("ps4mods", "Imported %d add-on(s) from '%s', %d failed.",
              installed, source.c_str(), failed);
    return false;
}   // importRoots

void importZip(const std::string& source, ImportContext& context);

// A "mod pack" can be a folder or a zip that contains individual add-on zips
// instead of unpacked kart/track folders.  Scan those zips, but only when the
// outer source did not itself contain an add-on: a valid kart that happens to
// ship a texture zip must not be unpacked as another mod.
int importNestedZips(const std::string& dir, unsigned int depth,
                     ImportContext& context)
{
    if (depth > kMaxScanDepth)
        return 0;

    std::vector<std::string> entries;
    if (!listDirectory(dir, entries))
        return 0;

    int imported = 0;
    for (std::vector<std::string>::const_iterator it = entries.begin();
         it != entries.end(); ++it)
    {
        if (isSkippedName(*it))
            continue;
        const std::string child = joinPath(dir, *it);
        if (isZipName(*it) && isRegularFile(child))
        {
            const int before = context.installed;
            importZip(child, context);
            if (context.installed > before)
                imported++;
        }
        else if (isDirectory(child))
        {
            imported += importNestedZips(child, depth + 1, context);
        }
    }
    return imported;
}   // importNestedZips

bool zipHasAddon(const std::string& path)
{
    if (!irr_driver || !irr_driver->getDevice())
        return false;

    irr::io::IFileSystem* const files =
        irr_driver->getDevice()->getFileSystem();
    if (!files)
        return false;

    // Keep the exact archive pointer: addFileArchive may report success for an
    // archive that is already mounted, and remove-by-name can then target a
    // different entry.  A pack zip has no kart.xml itself but contains nested
    // add-on zips, so accept that shape too and let importNestedZips inspect it.
    irr::io::IFileArchive* archive = NULL;
    if (!files->addFileArchive(path.c_str(), /*ignoreCase*/false,
                               /*ignorePath*/false, irr::io::EFAT_ZIP,
                               irr::core::stringc(), &archive))
        return false;

    bool found = false;
    bool nested_zip = false;
    const irr::io::IFileList* const list =
        archive ? archive->getFileList() : NULL;
    if (list)
    {
        for (unsigned int i = 0; i < list->getFileCount(); i++)
        {
            if (list->isDirectory(i))
                continue;
            const std::string name =
                baseName(std::string(list->getFileName(i).c_str()));
            if (name == "kart.xml" || name == "track.xml")
            {
                found = true;
                break;
            }
            if (isZipName(name))
                nested_zip = true;
        }
    }

    if (archive)
        files->removeFileArchive(archive);
    return found || nested_zip;
}   // zipHasAddon

void importZip(const std::string& source, ImportContext& context)
{
    if (!context.seen.insert(source).second)
        return;

    std::string stamp;
    if (!sourceStamp(source, stamp))
    {
        Log::warn("ps4mods", "Could not read mod archive '%s'.", source.c_str());
        return;
    }
    if (alreadyImported(source, stamp, context))
        return;

    if (!zipHasAddon(source))
    {
        Log::info("ps4mods", "Archive '%s' contains no kart.xml, "
                  "track.xml, or nested add-on zip.", source.c_str());
        return;
    }

    static unsigned int counter = 0;
    char suffix[64];
    snprintf(suffix, sizeof(suffix), "/.tmp/import-%ld-%u",
             (long)getpid(), counter++);
    const std::string staging = std::string(kInternalMods) + suffix;

    if (!file_manager->checkAndCreateDirectoryP(staging))
    {
        Log::error("ps4mods", "Could not create staging directory '%s'.",
                   staging.c_str());
        context.failed++;
        return;
    }

    bool extracted = false;
    try
    {
        extracted = extract_zip(source, staging, true);
    }
    catch (const std::exception& error)
    {
        Log::warn("ps4mods", "Exception extracting '%s': %s",
                  source.c_str(), error.what());
    }
    catch (...)
    {
        Log::warn("ps4mods", "Exception extracting '%s'.", source.c_str());
    }
    if (!extracted)
    {
        // Do not publish a half-extracted kart: a missing model or texture
        // produces a broken entry that is worse than retrying next boot.
        Log::warn("ps4mods", "Archive '%s' did not extract cleanly; "
                  "not installing it.", source.c_str());
        removeTree(staging);
        context.failed++;
        return;
    }

    std::vector<AddonRoot> roots;
    collectAddonRoots(staging, 0, roots);
    bool imported = false;
    if (!roots.empty())
    {
        for (std::vector<AddonRoot>::iterator it = roots.begin();
             it != roots.end(); ++it)
        {
            // A zip may contain kart.xml/track.xml directly at its root.  In
            // that case there is no folder name to use, so name the add-on
            // after the archive instead of the temporary staging directory.
            if (it->path == staging)
                it->use_fallback_name = true;
        }

        std::string archive_name = baseName(source);
        if (archive_name.size() >= 4)
            archive_name.erase(archive_name.size() - 4);
        imported = importRoots(source, roots,
                               safeDirectoryName(archive_name), context);
    }
    else
    {
        imported = importNestedZips(staging, 0, context) > 0;
        if (!imported)
            Log::info("ps4mods", "Archive '%s' contains no kart.xml, "
                      "track.xml, or nested add-on zip.", source.c_str());
    }

    removeTree(staging);

    if (imported)
    {
        context.imported[source] = stamp;
        context.state_dirty = true;
    }
}   // importZip

void importFolder(const std::string& source, ImportContext& context)
{
    if (!context.seen.insert(source).second)
        return;

    std::string stamp;
    if (!sourceStamp(source, stamp))
    {
        Log::warn("ps4mods", "Could not read mod folder '%s'.", source.c_str());
        return;
    }
    if (alreadyImported(source, stamp, context))
        return;

    std::vector<AddonRoot> roots;
    collectAddonRoots(source, 0, roots);
    if (!roots.empty())
    {
        if (importRoots(source, roots, baseName(source), context))
        {
            context.imported[source] = stamp;
            context.state_dirty = true;
        }
        return;
    }

    // A folder can be a pack of individual add-on zips rather than one
    // unpacked add-on.  This is the "drop a folder of downloads" case.
    if (importNestedZips(source, 0, context) == 0)
        Log::info("ps4mods", "No kart.xml, track.xml, or nested add-on zip "
                  "found in '%s'.", source.c_str());
}   // importFolder

// ----------------------------------------------------------------------------
// Directory scanning
// ----------------------------------------------------------------------------

void scanModsDirectory(const std::string& dir, ImportContext& context)
{
    if (!isDirectory(dir))
        return;

    std::vector<std::string> entries;
    if (!listDirectory(dir, entries))
        return;

    for (std::vector<std::string>::const_iterator it = entries.begin();
         it != entries.end(); ++it)
    {
        if (isSkippedName(*it))
            continue;
        const std::string child = joinPath(dir, *it);
        if (isZipName(*it) && isRegularFile(child))
            importZip(child, context);
        else if (isDirectory(child))
            importFolder(child, context);
    }
}   // scanModsDirectory

void scanLooseDirectory(const std::string& dir, ImportContext& context)
{
    if (!isDirectory(dir))
        return;

    std::vector<std::string> entries;
    if (!listDirectory(dir, entries))
        return;

    for (std::vector<std::string>::const_iterator it = entries.begin();
         it != entries.end(); ++it)
    {
        if (isSkippedName(*it))
            continue;
        const std::string child = joinPath(dir, *it);
        if (isZipName(*it) && isRegularFile(child))
        {
            importZip(child, context);
        }
        else if (isDirectory(child))
        {
            // At the drive root, only an actual add-on folder or a tracks/
            // karts/ collection is considered.  This avoids walking unrelated
            // game backups and saves.
            if (addonKindAt(child) != ADDON_NONE ||
                *it == "tracks" || *it == "karts")
                importFolder(child, context);
        }
    }
}   // scanLooseDirectory

void scanUsbRoot(const std::string& root, ImportContext& context)
{
    if (!isDirectory(root))
        return;

    // The common layouts people naturally make for a console mod drop.
    const char* const mod_dirs[] = {
        "mods",
        "Mods",
        "MODS",
        "stk-mods",
        "addons",
        "SuperTuxKart/mods",
        "SuperTuxKart/Mods",
        "SuperTuxKart/addons",
        "supertuxkart/mods",
        "supertuxkart/addons",
        "STK/mods",
        "STK/Mods",
        "STK/addons",
        "stk/addons",
        "PS4/mods",
        "PS4/Mods",
        "PS4/SuperTuxKart/mods",
        "PS4/SuperTuxKart/addons",
        "PS4/STK/mods",
        "PS4/STK/addons"
    };
    for (unsigned int i = 0; i < sizeof(mod_dirs) / sizeof(mod_dirs[0]); i++)
        scanModsDirectory(joinPath(root, mod_dirs[i]), context);

    const char* const containers[] = {
        "SuperTuxKart",
        "supertuxkart",
        "STK",
        "stk"
    };
    for (unsigned int i = 0; i < sizeof(containers) / sizeof(containers[0]); i++)
        scanLooseDirectory(joinPath(root, containers[i]), context);

    // A zip or an unpacked add-on dropped at the drive root also works.
    scanLooseDirectory(root, context);
}   // scanUsbRoot

void scanInternal(ImportContext& context)
{
    scanModsDirectory(kInternalMods, context);

    const char* const other_dirs[] = {
        "/data/stk/addons",
        "/data/mods",
        "/data/addons",
        "/data/SuperTuxKart/mods",
        "/data/supertuxkart/mods",
        "/data/STK/mods"
    };
    for (unsigned int i = 0;
         i < sizeof(other_dirs) / sizeof(other_dirs[0]); i++)
        scanModsDirectory(other_dirs[i], context);
}   // scanInternal

bool ensureInternalModsDirectory()
{
    struct stat info;
    if (lstat(kInternalMods, &info) == 0)
        return S_ISDIR(info.st_mode);

    if (mkdir(kInternalMods, 0777) == 0 || errno == EEXIST)
        return true;
    return false;
}   // ensureInternalModsDirectory

}   // namespace

// ----------------------------------------------------------------------------
void ps4_install_mods()
{
    if (!file_manager || !irr_driver || !irr_driver->getDevice())
    {
        Log::warn("ps4mods", "Skipping mod import: game is not initialised.");
        return;
    }

    Log::info("ps4mods", "Add-ons directory: %s",
              file_manager->getAddonsDir().c_str());
    if (!ensureInternalModsDirectory())
    {
        Log::warn("ps4mods", "Could not create '%s'.", kInternalMods);
        return;
    }

    ImportContext context;
    loadState(context);
    scanInternal(context);
    for (int i = 0; i < 8; i++)
        scanUsbRoot("/mnt/usb" + std::to_string(i), context);

    if (context.state_dirty)
        saveState(context);

    Log::info("ps4mods", "Mod import finished: %d installed, %d failed.",
              context.installed, context.failed);
    ps4_boot_tracef("port: mods: %d installed, %d failed, addons %s",
                    context.installed, context.failed,
                    file_manager->getAddonsDir().c_str());
}   // ps4_install_mods
