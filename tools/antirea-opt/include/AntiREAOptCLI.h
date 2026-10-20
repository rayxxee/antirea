/**
 * AntiREAOptCLI.h — antirea-opt internal interface
 */

#pragma once

namespace mirage {

/**
 * Main entry point for antirea-opt.
 * Returns 0 on success, non-zero on error.
 */
int antireaOptMain(int argc, char **argv);

} /* namespace mirage */
