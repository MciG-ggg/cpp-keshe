#pragma once

#define CROW_MAIN
#define CROW_USE_BOOST_ASIO
#include <boost/asio.hpp>
#include "crow.h"
#include "crow/middlewares/cors.h"
#include "parking_lot.h"
#include <memory>
#include <nlohmann/json.hpp>

class CrowParkingServer {
public:
    CrowParkingServer(size_t capacity = 100, 
                     double smallRate = 5.0, 
                     double largeRate = 8.0,
                     const std::string& dataFile = "parking_data.dat");

    void start(uint16_t port = 8080);

private:
    void setupRoutes();
    void setupCORS();

    std::shared_ptr<ParkingLot> parkingLot;
    crow::SimpleApp app;
};
