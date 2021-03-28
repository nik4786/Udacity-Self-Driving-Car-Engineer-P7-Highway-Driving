/* 
* File name: calculateCost.h
* Author: Nijinshan Karunainayagam
* Date created: 25.03.2021 
* Date last modified: 28.03.2021
*
* contains cost calculation for states
*/
#include <iostream>
#include <string>
#include <vector>
#include "helpers.h"

using std::string;

double cruise_cost(double minDist) {
  double cost;
  if (minDist < 15) {
    cost = 999;
  } else if (minDist == 999) {
    cost = 0;
  } else {
    cost = 50;
  } 
  return cost;
}

double laneChangeLeft_cost (double carLane, double lfMin, double lbMin) {
  double cost;
  if ((carLane == 0) || (lfMin < 30) || (lbMin<15)) {
    cost = 1000;
  } else if (lfMin == 999 && lbMin == 999) {
    cost = 1;
  } else if (lfMin == 999 && lbMin > 15 && lbMin < 30) {
    cost = 40;
  } else {
    cost = 60;
  }
  return cost;
}

double laneChangeRight_cost (double carLane, double rfMin, double rbMin) {
  double cost;
  if (carLane == 2 || rfMin < 30 || rbMin < 15) {
    cost = 999;
  } else if (rfMin == 999 && rbMin == 999) {
    cost = 1;
  } else if (rfMin == 999 && rbMin > 15 && rbMin < 30) {
    cost = 40;
  } else {
    cost = 60;
  }
  return cost;
}
  
        