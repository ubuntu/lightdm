/*
 * Unity build compilation unit for liblightdm-qt.
 * Amalgamates greeter.cpp, power.cpp, sessionsmodel.cpp, and usersmodel.cpp
 * into a single compilation unit to avoid repeated parsing of Qt/GLib headers
 * and duplicate template instantiations.
 */

#include "greeter.cpp"
#include "power.cpp"
#include "sessionsmodel.cpp"
#include "usersmodel.cpp"
