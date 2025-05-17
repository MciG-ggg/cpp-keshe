#include "vehicle.h"

Vehicle::Vehicle(const std::string& plate, const std::string& vType)
    : licensePlate(plate)
    , type(vType)
    , entryTime(std::time(nullptr))
    , exitTime(0)
    , fee(0.0)
{
}

std::string Vehicle::getLicensePlate() const {
    return licensePlate;
}

std::string Vehicle::getType() const {
    return type;
}

time_t Vehicle::getEntryTime() const {
    return entryTime;
}

time_t Vehicle::getExitTime() const {
    return exitTime;
}

double Vehicle::getFee() const {
    return fee;
}

void Vehicle::checkout() {
    if (exitTime == 0) {
        exitTime = std::time(nullptr);
    }
}

double Vehicle::calculateFee(time_t currentTime) const {
    time_t endTime = exitTime > 0 ? exitTime : currentTime;
    double hours = std::difftime(endTime, entryTime) / 3600.0;
    return hours;
}

void Vehicle::setFee(double newFee) {
    fee = newFee;
}

void Vehicle::setEntryTime(time_t time) {
    entryTime = time;
}

void Vehicle::setExitTime(time_t time) {
    exitTime = time;
}