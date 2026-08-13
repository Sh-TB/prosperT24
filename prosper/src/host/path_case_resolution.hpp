/**
 * Case-Correct Path Resolution for Prosper PS4 Emulator
 *
 * Resolves paths with incorrect casing to their actual on-disk counterparts.
 * Critical for Linux hosts where filesystem is case-sensitive but PS4 games
 * may reference modules with varying case (e.g., "Il2cpp" vs "Il2Cpp").
 *
 * Problem (#1006):
 * - Games like The Messenger ship "Il2cppUserAssemblies.prx"
 * - Blasphemous 2 / Evergate ship "Il2CppUserAssemblies.prx"
 * - On case-sensitive Linux FS, wrong casing causes silent module drop
 * - Result: ENOENT → null-jump → SIGSEGV at rip=0
 *
 * Solution:
 * - Recursively correct each path component against directory entries
 * - Preserve original path if no match found (caller handles absence)
 * - Never throws; uses error_code overloads throughout
 *
 * @upstream-candidate Ready for review
 * @see boot_program.cpp for primary integration point
 * @author Prosper Team
 */

#pragma once

#include <string>
#include <vector>

namespace prosper {

/**
 * Resolve a path to its correct on-disk casing.
 *
 * For each component of the path (from root to leaf):
 * 1. If the exact-cased path exists, return it immediately
 * 2. Otherwise, recursively resolve parent directory
 * 3. Search parent's entries for case-insensitive match
 * 4. If match found, return corrected path
 * 5. If no match, return best-effort correction (parent may be corrected)
 *
 * Behavior on different filesystem types:
 * - Case-sensitive (ext4, etc.): Corrects mismatched components
 * - Case-insensitive (NTFS, APFS): Returns input unchanged (already works)
 *
 * @param want  The requested path (may have wrong casing)
 * @return      Path with corrected casing, or original if no correction possible
 *
 * @note This function never throws. All filesystem errors are handled internally.
 * @note Empty input returns empty string.
 * @note Absent files return input unchanged (caller's absence check still triggers).
 *
 * Example:
 * @code
 *   // Disk has: /game/Media/Modules/Il2CppUserAssemblies.prx
 *   // Request:  /game/Media/Modules/Il2cppUserAssemblies.prx
 *   // Returns:  /game/Media/Modules/Il2CppUserAssemblies.prx  (corrected)
 * @endcode
 */
std::string resolve_host_path_case(const std::string& want);

/**
 * Discover extra plugin modules not in the fixed preload list.
 *
 * Scans Media/Plugins directory for .prx files that aren't already
 * in the provided basename list. This enables auto-linking of
 * game-specific plugins without hardcoding each one.
 *
 * @param dump_root          Game dump root directory
 * @param listed_basenames   Already-listed module basenames (lowercase)
 * @return                   Paths to newly discovered .prx modules
 *
 * @note Results sorted descending by lowercase basename (for reverse init order).
 * @note Skips non-regular files and non-.prx extensions.
 */
std::vector<std::string> discover_extra_plugin_modules(
    const std::string& dump_root,
    const std::vector<std::string>& listed_basenames);

// ============================================================================
// Testing Support (not part of public API, exposed for unit tests)
// ============================================================================

namespace testing {
namespace path_case {

/**
 * Case-insensitive string comparison helper.
 * Only matches if strings have same length and differ only by case.
 */
bool case_insensitive_equal(const std::string& a, const std::string& b);

} // namespace path_case
} // namespace testing

} // namespace prosper
