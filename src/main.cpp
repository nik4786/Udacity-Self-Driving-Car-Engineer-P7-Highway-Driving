/* 
* File name: main.cpp
* Author: Nijinshan Karunainayagam
* Date created: 24.03.2021 
* Date last modified: 28.03.2021
*
* main file of highway driving project
*/
#include <uWS/uWS.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "helpers.h"
#include "json.hpp"
#include "calculateCost.h"

//external reference available in https://kluge.in-chemnitz.de/opensource/spline/spline.h
#include "spline.h"  

// for convenience
using nlohmann::json;
using std::string;
using std::vector;

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  std::ifstream in_map_(map_file_.c_str(), std::ifstream::in);

  string line;
  while (getline(in_map_, line)) {
    std::istringstream iss(line);
    double x;
    double y;
    float s;
    float d_x;
    float d_y;
    iss >> x;
    iss >> y;
    iss >> s;
    iss >> d_x;
    iss >> d_y;
    map_waypoints_x.push_back(x);
    map_waypoints_y.push_back(y);
    map_waypoints_s.push_back(s);
    map_waypoints_dx.push_back(d_x);
    map_waypoints_dy.push_back(d_y);
  }

  // define init lane and velocity
  int lane = 1;
  double ref_vel = 0.0;
  
  h.onMessage([&map_waypoints_x,&map_waypoints_y,&map_waypoints_s,
               &map_waypoints_dx,&map_waypoints_dy,&lane,&ref_vel]
              (uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
               uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
          // Main car's localization Data
          double car_x = j[1]["x"];
          double car_y = j[1]["y"];
          double car_s = j[1]["s"];
          double car_d = j[1]["d"];
          double car_yaw = j[1]["yaw"];
          double car_speed = j[1]["speed"];

          // Previous path data given to the Planner
          auto previous_path_x = j[1]["previous_path_x"];
          auto previous_path_y = j[1]["previous_path_y"];
          // Previous path's end s and d values 
          double end_path_s = j[1]["end_path_s"];
          double end_path_d = j[1]["end_path_d"];

          // Sensor Fusion Data, a list of all other cars on the same side 
          //   of the road.
          auto sensor_fusion = j[1]["sensor_fusion"];      
          
          
          int prev_size = previous_path_x.size(); // get size of previous path array 
          // if array is not empty, set end path s-value as ego's s-value 
          if(prev_size > 0) {
            car_s = end_path_s;
          }
          
          
          /*
          * INITIALIZATION OF VARIABLES
          */
          double dist;
          double minDist;
          double step_time = 0.02; // step time
          double t = prev_size * step_time; // prediction time
          double MAX_ACC = 9.0; // max acceleration
          double MAX_VEL = 49.5 * (1600.0 / 3600.0); // max velocity in m/s
          bool criticalDistance = false; // check, if target is too close
          double trackedObj_FL = 999; // object tracked front left
          double trackedObj_RL = 999; // object tracker rear left
          double trackedObj_Front = 999; // object tracked front
          double trackedObj_Rear = 999; // object tracked rear                     
          double trackedObj_FR = 999; // object tracked front right
          double trackedObj_RR = 999; // object tracked rear right 
          double vel_FL = 999; // velocity of front left object
          double vel_RL = 999; // velocity of rear left object
          double vel_front = 999; // velocity of lead object
		  double vel_rear = 999; // velocity of object behind
          double vel_FR = 999; // velocity of front right object
          double vel_RR = 999; // velocity of rear right object
          double lookAhead = 30; // lookahead distance: 44.44 m (2 second rule for 50 mph)
          double lookBehind = 25; // lookbehind distance
          double actionAhead = 30;  // init action distance rear
          double actionBehind = 30; // init action distance behind
          double safetyDistance = 22.22; // safety distance to front target
          double car_speed_ahead = -1; // velocity of lead target
          double closestLeadDistance = lookAhead; // distance to closest lead
          
          /*
          * LOGIC TO CHECK EGO'S ENVIRONMENT
          */
          for (int i = 0;  i < sensor_fusion.size(); ++i)
          {                       
            // Get all information of sensor fusion data 
            // and declare them
            double d = sensor_fusion[i][6];   
            int target_ID = sensor_fusion[i][0];
            double target_PosX = sensor_fusion[i][1];
            double target_PosY = sensor_fusion[i][2];
            double target_VelX = sensor_fusion[i][3];
            double target_VelY = sensor_fusion[i][4];
            double target_Vel = sqrt(target_VelX*target_VelX + target_VelY*target_VelY);
            double target_s = sensor_fusion[i][5];
            double target_d = sensor_fusion[i][6];
            double target_lane = findLaneID(target_d);
            double ego_lane = findLaneID(car_d);
            double distance_s = target_s - car_s;                 
            // Check, in which lane the target is and the position
            // w.r.t. ego.
  		    if (target_lane == ego_lane) {
              dist = target_s - car_s;
              if (dist > 0 && dist < lookAhead && dist < trackedObj_Front) {
                trackedObj_Front = dist;
                vel_front = target_Vel;
              }
            } else if (ego_lane != 0 && target_lane == ego_lane-1) {
              dist = target_s - car_s;
              if (dist > 0 && dist < lookAhead+0.265 && dist < trackedObj_FL) {
                trackedObj_FL = dist;
                vel_FL = target_Vel;
              } else if (dist < 0 && dist > -lookAhead-0.265 && std::abs(dist) < trackedObj_RL) {
                trackedObj_RL = std::abs(dist);
                vel_RL = target_Vel;
              }
            } else if (ego_lane != 2 && target_lane == ego_lane+1) {
              dist = target_s - car_s;
              if (dist > 0 && dist < lookAhead+0.265 && dist < trackedObj_FR) {
                trackedObj_FR = dist;
                vel_FR = target_Vel;
              } else if (dist < 0 && dist > -lookAhead-0.265 && std::abs(dist) < trackedObj_RR) {
                trackedObj_RR = std::abs(dist);
                vel_RR = target_Vel;
              }
            }
          }
          
          /*
          * FINITE STATE MACHINE FOR BEHAVIORAL PLANNING
          *
          * CRUISE: no lead vehicle, drive with speed of 50 mph
          * LANE_CHANGE_LEFT: Switch to left lane (from current lane)
          * LANE_CHANGE_RIGHT: Switch to right lane (from current lane)
          */
          int ego_lane = findLaneID(car_d); // ego's current lane
          double minimalCost = 9999; // init cost
          string state = "null"; // init state
          // get cruise cost and check, if it is the smallest cost
          double cruiseCost = cruise_cost(trackedObj_Front);
          if (cruiseCost < minimalCost) {
            minimalCost = cruiseCost;
            state = "CRUISE";
          }
          // get lane change left cost and check, if it is the smallest cost
          double laneChangeLCost = laneChangeLeft_cost(ego_lane, trackedObj_FL, trackedObj_RL);
          if (laneChangeLCost < minimalCost) {
            minimalCost = laneChangeLCost;
            state = "LANE_CHANGE_LEFT";
          }
          // get lane change right cost and check, if it is the smallest cost
          double laneChangeRCost = laneChangeRight_cost(ego_lane, trackedObj_FR, trackedObj_RR);
          if (laneChangeRCost < minimalCost) {
            minimalCost = laneChangeRCost;
            state = "LANE_CHANGE_RIGHT";
          }

          /*
          * DETERMINE PARAMETERS FOR TRAJECTORY GENERATION
          */
          double delta_v;
          double goal_d;
          double goal_s;
          // determine goal d-coordinate and speed difference for different states
          if (state == "CRUISE") {
            ego_lane = ego_lane;
            goal_d = 2 + ego_lane * 4;
            delta_v = vel_front - ref_vel;
            goal_s = trackedObj_Front;
          } else if (state == "LANE_CHANGE_LEFT") {
            ego_lane = ego_lane - 1;
            goal_d = 2 + ego_lane * 4;
            delta_v = vel_FL - ref_vel;
            goal_s = trackedObj_FL;
          } else if (state == "LANE_CHANGE_RIGHT") {
            ego_lane = ego_lane + 1;
            goal_d = 2 + ego_lane * 4;
            delta_v = vel_FR - ref_vel;
            goal_s = trackedObj_FR;
          }
          // calculate proper speed difference w.r.t. maximal acceleration
          if (goal_s == 999 && delta_v > 0) {
            delta_v = MAX_ACC * step_time;
          } else if (delta_v < 0) {
            delta_v = (-MAX_ACC * step_time);
          } else {
            delta_v = 0;
          }
          //std::cout << "CURRENT LANE:" << ego_lane << std::endl;
          //std::cout << "GOAL D:" << goal_d << std::endl;
        
          json msgJson;

          /*
          * TRAJECTORY GENERATION
          */
     
          // create a list of widely spaced (x,y) waypoints, evenly spaced at 30m
          // later we will interpolate theses waypoints with a spline and fill it in with more points that control speed.          
          vector<double> ptsx;
          vector<double> ptsy;
          //reference x,y and yaw states
          //either we will reference the starting point as where the car is or at the previous path end point
          double ref_x = car_x;
          double ref_y = car_y;
          double ref_yaw = deg2rad(car_yaw);
          //if previous size is almost empty, use the car as starting reference
          if (prev_size < 2) {
            //Use two points that make the path tangent to the car
            double prev_car_x = car_x - cos(car_yaw);
            double prev_car_y = car_y - sin(car_yaw);
            
            ptsx.push_back(prev_car_x);
            ptsx.push_back(car_x);
            
            ptsy.push_back(prev_car_y);
            ptsy.push_back(car_y);
          }
          else {
            //redefine reference state as previous path end point
            ref_x = previous_path_x[prev_size-1];
            ref_y = previous_path_y[prev_size-1];
            
            double ref_x_prev = previous_path_x[prev_size-2];
            double ref_y_prev = previous_path_y[prev_size-2];
            ref_yaw = atan2(ref_y - ref_y_prev,ref_x - ref_x_prev);
            
            // Use two points that make the path tangent to the previous Path's end point
            ptsx.push_back(ref_x_prev);
            ptsx.push_back(ref_x);
            
            ptsy.push_back(ref_y_prev);
            ptsy.push_back(ref_y);
          }
          
          //In frenet add evenly 30m spaced points ahead of the starting reference
          vector<double>next_wp0 = getXY(car_s+50,goal_d,map_waypoints_s,map_waypoints_x,map_waypoints_y);
          vector<double>next_wp1 = getXY(car_s+70,goal_d,map_waypoints_s,map_waypoints_x,map_waypoints_y);            
          vector<double>next_wp2 = getXY(car_s+90,goal_d,map_waypoints_s,map_waypoints_x,map_waypoints_y);  
          
          ptsx.push_back(next_wp0[0]);
          ptsx.push_back(next_wp1[0]);
          ptsx.push_back(next_wp2[0]);          
          
          ptsy.push_back(next_wp0[1]);
          ptsy.push_back(next_wp1[1]);
          ptsy.push_back(next_wp2[1]);     
          
          for (int i=0; i < ptsx.size(); i++) {
            // shift car reference angle to 0 degrees
            double shift_x = ptsx[i] - ref_x;
            double shift_y = ptsy[i] - ref_y;
            
            ptsx[i] = (shift_x * cos(0 - ref_yaw) - shift_y * sin (0 - ref_yaw));
            ptsy[i] = (shift_x * sin(0 - ref_yaw) + shift_y * cos (0 - ref_yaw));
          }
          
          //create a spline
          tk:: spline s;
          
          // set (x,y) points to the spline
          s.set_points(ptsx,ptsy);
          
          //Define the actual (x,y) points we will use for the planner
          vector<double> next_x_vals;
          vector<double> next_y_vals;
          
          //Start with all of the previous path points from last time
          for (int i = 0; i < previous_path_x.size(); i++) {
            next_x_vals.push_back(previous_path_x[i]);
            next_y_vals.push_back(previous_path_y[i]);          
          }
          // Calculate how to break up spline points so that we travel at our desired reference velocity
          double target_x = 30.0;
          double target_y = s(target_x);
          double target_dist = sqrt((target_x)*(target_x) + (target_y)*(target_y));
          
          double x_add_on = 0;
          
          //Fill up the rest of our path planner after filling it with previous points , here we will always output 50 points
          for (int i = 1; i <= 50 - previous_path_x.size(); i++) {
            // Set ego's velocity according to lead vehicle and speed limit
            ref_vel += delta_v;
			if (ref_vel > MAX_VEL) {
              ref_vel = MAX_VEL;
            }
            //std::cout << ref_vel << std::endl;
            double N = (target_dist/ (step_time * ref_vel));
            double x_point = x_add_on + (target_x) / N;
            double y_point = s(x_point);
            
            x_add_on = x_point;
            
            double x_ref = x_point;
            double y_ref = y_point;
            
            // rotate back to normal after rotating it earlier
            x_point = (x_ref * cos (ref_yaw) - y_ref * sin (ref_yaw));
            y_point = (x_ref * sin (ref_yaw) + y_ref * cos (ref_yaw));
            
            x_point += ref_x;
            y_point += ref_y;
            
            next_x_vals.push_back(x_point);
            next_y_vals.push_back(y_point);            
          }
       
          
//------------------------------------------------------------------------------------------------------
          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          auto msg = "42[\"control\","+ msgJson.dump()+"]";

          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }  // end "telemetry" if
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }  // end websocket if
  }); // end h.onMessage

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  
  h.run();
}