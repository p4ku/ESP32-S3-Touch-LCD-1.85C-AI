#!/bin/bash

# This script is used to kill the backend service for the ESP32-S3 Touch LCD project.

ps aux | grep 'run_service.sh' | grep bash | awk '{print $2}' | kill -9

# Kill the processes running on ports 8765 and 8766
# This is necessary to ensure that the service can be restarted without port conflicts.
sudo lsof -i :8765 | awk 'NR>1 {print $2}' | xargs kill -9
sudo lsof -i :8766 | awk 'NR>1 {print $2}' | xargs kill -9
