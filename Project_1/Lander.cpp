/*
	Lander Control simulation.

	Updated by F. Estrada for CSC C85, Oct. 2013
	Updated by Per Parker, Sep. 2015

	Learning goals:

	- To explore the implementation of control software
	  that is robust to malfunctions/failures.

	The exercise:

	- The program loads a terrain map from a .ppm file.
	  the map shows a red platform which is the location
	  a landing module should arrive at.
	- The control software has to navigate the lander
	  to this location and deposit the lander on the
	  ground considering:

	  * Maximum vertical speed should be less than 10 m/s at touchdown
	  * Maximum landing angle should be less than 15 degrees w.r.t vertical

	- Of course, touching any part of the terrain except
	  for the landing platform will result in destruction
	  of the lander

	This has been made into many videogames. The oldest one
	I know of being a C64 game called 1985 The Day After.
        There are older ones! (for bonus credit, find the oldest
        one and send me a description/picture plus info about the
        platform it ran on!)

	Your task:

	- These are the 'sensors' you have available to control
          the lander.

	  Velocity_X();  - Gives you the lander's horizontal velocity
	  Velocity_Y();	 - Gives you the lander's vertical velocity
	  Position_X();  - Gives you the lander's horizontal position (0 to 1024)
	  Position Y();  - Gives you the lander's vertical position (0 to 1024)

          Angle();	 - Gives the lander's angle w.r.t. vertical in DEGREES (upside-down = 180 degrees)

	  SONAR_DIST[];  - Array with distances obtained by sonar. Index corresponds
                           to angle w.r.t. vertical direction measured clockwise, so that
                           SONAR_DIST[0] is distance at 0 degrees (pointing upward)
                           SONAR_DIST[1] is distance at 10 degrees from vertical
                           SONAR_DIST[2] is distance at 20 degrees from vertical
                           .
                           .
                           .
                           SONAR_DIST[35] is distance at 350 degrees from vertical

                           if distance is '-1' there is no valid reading. Note that updating
                           the sonar readings takes time! Readings remain constant between
                           sonar updates.

          RangeDist();   - Uses a laser range-finder to accurately measure the distance to ground
                           in the direction of the lander's main thruster.
                           The laser range finder never fails (probably was designed and
                           built by PacoNetics Inc.)

          Note: All sensors are NOISY. This makes your life more interesting.

	- Variables accessible to your 'in flight' computer

	  MT_OK		- Boolean, if 1 indicates the main thruster is working properly
	  RT_OK		- Boolean, if 1 indicates the right thruster is working properly
	  LT_OK		- Boolean, if 1 indicates thr left thruster is working properly
          PLAT_X	- X position of the landing platform
          PLAY_Y        - Y position of the landing platform

	- Control of the lander is via the following functions
          (which are noisy!)

	  Main_Thruster(double power);   - Sets main thurster power in [0 1], 0 is off
	  Left_Thruster(double power);	 - Sets left thruster power in [0 1]
	  Right_Thruster(double power);  - Sets right thruster power in [0 1]
	  Rotate(double angle);	 	 - Rotates module 'angle' degrees clockwise
					   (ccw if angle is negative) from current
                                           orientation (i.e. rotation is not w.r.t.
                                           a fixed reference direction).

 					   Note that rotation takes time!


	- Important constants

	  G_ACCEL = 8.87	- Gravitational acceleration on Venus
	  MT_ACCEL = 35.0	- Max acceleration provided by the main thruster
	  RT_ACCEL = 25.0	- Max acceleration provided by right thruster
	  LT_ACCEL = 25.0	- Max acceleration provided by left thruster
          MAX_ROT_RATE = .075    - Maximum rate of rotation (in radians) per unit time

	- Functions you need to analyze and possibly change

	  * The Lander_Control(); function, which determines where the lander should
	    go next and calls control functions
          * The Safety_Override(); function, which determines whether the lander is
            in danger of crashing, and calls control functions to prevent this.

	- You *can* add your own helper functions (e.g. write a robust thruster
	  handler, or your own robust sensor functions - of course, these must
	  use the noisy and possibly faulty ones!).

	- The rest is a black box... life sometimes is like that.

        - Program usage: The program is designed to simulate different failure
                         scenarios. Mode '1' allows for failures in the
                         controls. Mode '2' allows for failures of both
                         controls and sensors. There is also a 'custom' mode
                         that allows you to test your code against specific
                         component failures.

			 Initial lander position, orientation, and velocity are
                         randomized.

	  * The code I am providing will land the module assuming nothing goes wrong
          with the sensors and/or controls, both for the 'easy.ppm' and 'hard.ppm'
          maps.

	  * Failure modes: 0 - Nothing ever fails, life is simple
			   1 - Controls can fail, sensors are always reliable
			   2 - Both controls and sensors can fail (and do!)
			   3 - Selectable failure mode, remaining arguments determine
                               failing component(s):
                               1 - Main thruster
                               2 - Left Thruster
                               3 - Right Thruster
                               4 - Horizontal velocity sensor
                               5 - Vertical velocity sensor
                               6 - Horizontal position sensor
                               7 - Vertical position sensor
                               8 - Angle sensor
                               9 - Sonar

        e.g.

             Lander_Control easy.ppm 3 1 5 8

             Launches the program on the 'easy.ppm' map, and disables the main thruster,
             vertical velocity sensor, and angle sensor.

		* Note - while running. Pressing 'q' on the keyboard terminates the 
			program.

        * Be sure to complete the attached REPORT.TXT and submit the report as well as
          your code by email. Subject should be 'C85 Safe Landings, name_of_your_team'

	Have fun! try not to crash too many landers, they are expensive!

  	Credits: Lander image and rocky texture provided by NASA
		 Per Parker spent some time making sure you will have fun! thanks Per!
*/

/*
  Standard C libraries
*/
#include <math.h>

#include "Lander_Control.h"

/*
  Global Variables
*/

// Updates is sensors are working
// 0: X-vel
// 1: Y-vel
// 2: X-pos
// 3: Y-pos
// 4: Angle
// 5: Dist
bool broken[6] = {true, true, true, true, true, true};

// Store the last 5 values of each sensor
double xvel_sensor[5];
double yvel_sensor[5];
double xpos_sensor[5];
double ypos_sensor[5];
double angle_sensor[5];
double dist_sensor[5];
bool init_gv = false;

/*
  Helper functions
*/

// Returns the weighted average of a 5-array
double GetAvg(double (&arr)[5]) {
  double avg = 0;
  int count = 0;
  for (int i = 0; i < 5; i++) {
    count += 5 - i;
    avg += (5 - i) * arr[i];
  }
  return avg/count;
}

// Returns noise reduced value
double ReduceNoise(double avg, double sensor) {
  return (avg + 3 * sensor)/4;
}

// Returns if the input value is within delta of the avg
bool IsOutlier(double avg, double val, double delta) {
  return (val/avg > (1 + delta) || val/avg < (1 - delta)) ? false : true;
}

// Updates the history and returns the last value
double UpdateHistory(double val, double (&his)[5]) {
  for (int i = 4; i > 0; i--) {
      his[i] = his[i - 1];
    }
    his[0] = val;
    return val;
}

// Returns the X velocity
// Removes the noise, checks if the sensor is working
double Velocity_X_R() {
  // Get the weighted average from the recent history of X locations
  double avg = GetAvg(xvel_sensor);

  // Get the noisy input
  double x_sen = Velocity_X();

  // Get noise reduce value
  double x_r = ReduceNoise(avg, x_sen);

  // If the sensor is not broken
  if (!broken[0]) {
    // Check if the sensor is broken 
    // >25% difference from avg
    bool works = IsOutlier(avg, x_r, 0.25);

    // If the sensor works, update history and return sensor
    if (works) return UpdateHistory(x_r, xvel_sensor);

    // If the sensor is newly broken, update broken list
    broken[0] = false;
  }

  // If broken, try to use X position to determine velocity
  // If X position sensor is also not broken
  double pos_avg = GetAvg(xvel_sensor);
  double x_pos = ReduceNoise(pos_avg, Position_X());
  double x_r2 = UpdateHistory(x_pos - xpos_sensor[0], xvel_sensor);
  if (!broken[2]) return x_r2;

  // If both sensors are broken, return their average
  // This is beyond the scope of the problem
  return (x_r + x_r2)/2;
}

// Returns the Y velocity
// Removes the noise, checks if the sensor is working
double Velocity_Y_R() {
  // Get the weighted average from the recent history of X locations
  double avg = GetAvg(yvel_sensor);

  // Get the noisy input
  double y_sen = Velocity_Y();

  // Get noise reduce value
  double y_r = ReduceNoise(avg, y_sen);

  // If the sensor is not broken
  if (!broken[1]) {
    // Check if the sensor is broken 
    // >25% difference from avg
    bool works = IsOutlier(avg, y_r, 0.25);

    // If the sensor works, update history and return sensor
    if (works) return UpdateHistory(y_r, yvel_sensor);

    // If the sensor is newly broken, update broken list
    broken[1] = false;
  }

  // If broken, try to use X position to determine velocity
  // If X position sensor is also not broken
  double pos_avg = GetAvg(ypos_sensor);
  double y_pos = ReduceNoise(pos_avg, Position_Y());
  double y_r2 = UpdateHistory(y_pos - ypos_sensor[0], yvel_sensor);
  if (!broken[3]) return y_r2;

  // If both sensors are broken, return their average
  // This is beyond the scope of the problem
  return (y_r + y_r2)/2;
}

// Returns the X position
// Removes the noise, checks if the sensor is working
double Position_X_R() {
  // Get the weighted average from the recent history of X locations
  double avg = GetAvg(xpos_sensor);

  // Get the noisy input
  double x_sen = Position_X();

  // Get noise reduce value
  double x_r = ReduceNoise(avg, x_sen);

  // If the sensor is not broken
  if (!broken[2]) {
    // Check if the sensor is broken 
    // >25% difference from avg
    bool works = IsOutlier(avg, x_r, 0.25);

    // If the sensor works, update history and return sensor
    if (works) return UpdateHistory(x_r, xpos_sensor);

    // If the sensor is newly broken, update broken list
    broken[2]= false;
  }

  // If broken, try to use X velocity to determine position
  // If X position sensor is also not broken
  double vel_avg = GetAvg(xvel_sensor);
  double x_vel = ReduceNoise(vel_avg, Velocity_X());
  double x_r2 = UpdateHistory(xpos_sensor[0] + x_vel, xpos_sensor);
  if (!broken[0]) return x_r2;

  // If both sensors are broken, return their average
  // This is beyond the scope of the problem
  return (x_r + x_r2)/2;
}

// Returns the Y position
// Removes the noise, checks if the sensor is working
double Position_Y_R() {
  // Get the weighted average from the recent history of Y locations
  double avg = GetAvg(ypos_sensor);

  // Get the noisy input
  double y_sen = Position_Y();

  // Get noise reduce value
  double y_r = ReduceNoise(avg, y_sen);

  // If the sensor is not broken
  if (!broken[3]) {
    // Check if the sensor is broken 
    // >25% difference from avg
    bool works = IsOutlier(avg, y_r, 0.25);

    // If the sensor works, update history and return sensor
    if (works) return UpdateHistory(y_r, ypos_sensor);

    // If the sensor is newly broken, update broken list
    broken[3]= false;
  }

  // If broken, try to use X velocity to determine position
  // If X position sensor is also not broken
  double vel_avg = GetAvg(yvel_sensor);
  double y_vel = ReduceNoise(vel_avg, Velocity_Y());
  double y_r2 = UpdateHistory(ypos_sensor[0] + y_vel, ypos_sensor);
  if (!broken[0]) return y_r2;

  // If both sensors are broken, return their average
  // This is beyond the scope of the problem
  return (y_r + y_r2)/2;
}

void Lander_Control(void)
{
 /*
   This is the main control function for the lander. It attempts
   to bring the ship to the location of the landing platform
   keeping landing parameters within the acceptable limits.

   How it works:

   - First, if the lander is rotated away from zero-degree angle,
     rotate lander back onto zero degrees.
   - Determine the horizontal distance between the lander and
     the platform, fire horizontal thrusters appropriately
     to change the horizontal velocity so as to decrease this
     distance
   - Determine the vertical distance to landing platform, and
     allow the lander to descend while keeping the vertical
     speed within acceptable bounds. Make sure that the lander
     will not hit the ground before it is over the platform!

   As noted above, this function assumes everything is working
   fine.
*/

/*************************************************
 TO DO: Modify this function so that the ship safely
        reaches the platform even if components and
        sensors fail!

        Note that sensors are noisy, even when
        working properly.

        Finally, YOU SHOULD provide your own
        functions to provide sensor readings,
        these functions should work even when the
        sensors are faulty.

        For example: Write a function Velocity_X_robust()
        which returns the module's horizontal velocity.
        It should determine whether the velocity
        sensor readings are accurate, and if not,
        use some alternate method to determine the
        horizontal velocity of the lander.

        NOTE: Your robust sensor functions can only
        use the available sensor functions and control
        functions!
	DO NOT WRITE SENSOR FUNCTIONS THAT DIRECTLY
        ACCESS THE SIMULATION STATE. That's cheating,
        I'll give you zero.
**************************************************/

  if (!init_gv) {
    for (int i = 0; i< 5; i++) {
      xvel_sensor[i] = Velocity_X();
      yvel_sensor[i] = Velocity_Y();
      xpos_sensor[i] = Position_X();
      ypos_sensor[i] = Position_Y();
      angle_sensor[i] = Angle();
      dist_sensor[i] = RangeDist();
    }
    init_gv = true;
  }

  double VXlim;
  double VYlim;

  // Set velocity limits depending on distance to platform.
  // If the module is far from the platform allow it to
  // move faster, decrease speed limits as the module
  // approaches landing. You may need to be more conservative
  // with velocity limits when things fail.
  double xpr = Position_X_R();
  if (fabs(xpr-PLAT_X)>200) VXlim=25;
  else if (fabs(xpr-PLAT_X)>100) VXlim=15;
  else VXlim=5;

  double ypr = Position_Y_R();
  if (PLAT_Y-ypr>200) VYlim=-20;
  else if (PLAT_Y-ypr>100) VYlim=-10;  // These are negative because they
  else VYlim=-4;				       // limit descent velocity

  // Ensure we will be OVER the platform when we land
  double xvr = Velocity_X_R();
  double yvr = Velocity_Y_R();
  if (fabs(PLAT_X-xpr)/fabs(xvr)>1.25*fabs(PLAT_Y-ypr)/fabs(yvr)) VYlim=0;

  // IMPORTANT NOTE: The code below assumes all components working
  // properly. IT MAY OR MAY NOT BE USEFUL TO YOU when components
  // fail. More likely, you will need a set of case-based code
  // chunks, each of which works under particular failure conditions.

  // Check for rotation away from zero degrees - Rotate first,
  // use thrusters only when not rotating to avoid adding
  // velocity components along the rotation directions
  // Note that only the latest Rotate() command has any
  // effect, i.e. the rotation angle does not accumulate
  // for successive calls.

  if (Angle()>1&&Angle()<359)
  {
    if (Angle()>=180) Rotate(360-Angle());
    else Rotate(-Angle());
    return;
  }

  // Module is oriented properly, check for horizontal position
  // and set thrusters appropriately.
  if (xpr>PLAT_X)
  {
    // Lander is to the LEFT of the landing platform, use Right thrusters to move
    // lander to the left.
    Left_Thruster(0);	// Make sure we're not fighting ourselves here!
    if (xvr>(-VXlim)) Right_Thruster((VXlim+fmin(0,xvr))/VXlim);
    else
    {
    // Exceeded velocity limit, brake
    Right_Thruster(0);
    Left_Thruster(fabs(VXlim-xvr));
    }
  }
  else
  {
    // Lander is to the RIGHT of the landing platform, opposite from above
    Right_Thruster(0);
    if (xvr<VXlim) Left_Thruster((VXlim-fmax(0,xvr))/VXlim);
    else
    {
    Left_Thruster(0);
    Right_Thruster(fabs(VXlim-xvr));
    }
  }

  // Vertical adjustments. Basically, keep the module below the limit for
  // vertical velocity and allow for continuous descent. We trust
  // Safety_Override() to save us from crashing with the ground.
  if (yvr<VYlim) Main_Thruster(1.0);
  else Main_Thruster(0);
}

void Safety_Override(void)
{
 /*
   This function is intended to keep the lander from
   crashing. It checks the sonar distance array,
   if the distance to nearby solid surfaces and
   uses thrusters to maintain a safe distance from
   the ground unless the ground happens to be the
   landing platform.

   Additionally, it enforces a maximum speed limit
   which when breached triggers an emergency brake
   operation.
 */

/**************************************************
 TO DO: Modify this function so that it can do its
        work even if components or sensors
        fail
**************************************************/

/**************************************************
  How this works:
  Check the sonar readings, for each sonar
  reading that is below a minimum safety threshold
  AND in the general direction of motion AND
  not corresponding to the landing platform,
  carry out speed corrections using the thrusters
**************************************************/

 double DistLimit;
 double Vmag;
 double dmin;

 // Establish distance threshold based on lander
 // speed (we need more time to rectify direction
 // at high speed)
 Vmag=Velocity_X()*Velocity_X();
 Vmag+=Velocity_Y()*Velocity_Y();

 DistLimit=fmax(75,Vmag);

 // If we're close to the landing platform, disable
 // safety override (close to the landing platform
 // the Control_Policy() should be trusted to
 // safely land the craft)
 if (fabs(PLAT_X-Position_X())<150&&fabs(PLAT_Y-Position_Y())<150) return;

 // Determine the closest surfaces in the direction
 // of motion. This is done by checking the sonar
 // array in the quadrant corresponding to the
 // ship's motion direction to find the entry
 // with the smallest registered distance

 // Horizontal direction.
 dmin=1000000;
 if (Velocity_X()>0)
 {
  for (int i=5;i<14;i++)
   if (SONAR_DIST[i]>-1&&SONAR_DIST[i]<dmin) dmin=SONAR_DIST[i];
 }
 else
 {
  for (int i=22;i<32;i++)
   if (SONAR_DIST[i]>-1&&SONAR_DIST[i]<dmin) dmin=SONAR_DIST[i];
 }
 // Determine whether we're too close for comfort. There is a reason
 // to have this distance limit modulated by horizontal speed...
 // what is it?
 if (dmin<DistLimit*fmax(.25,fmin(fabs(Velocity_X())/5.0,1)))
 { // Too close to a surface in the horizontal direction
  if (Angle()>1&&Angle()<359)
  {
   if (Angle()>=180) Rotate(360-Angle());
   else Rotate(-Angle());
   return;
  }

  if (Velocity_X()>0){
   Right_Thruster(1.0);
   Left_Thruster(0.0);
  }
  else
  {
   Left_Thruster(1.0);
   Right_Thruster(0.0);
  }
 }

 // Vertical direction
 dmin=1000000;
 if (Velocity_Y()>5)      // Mind this! there is a reason for it...
 {
  for (int i=0; i<5; i++)
   if (SONAR_DIST[i]>-1&&SONAR_DIST[i]<dmin) dmin=SONAR_DIST[i];
  for (int i=32; i<36; i++)
   if (SONAR_DIST[i]>-1&&SONAR_DIST[i]<dmin) dmin=SONAR_DIST[i];
 }
 else
 {
  for (int i=14; i<22; i++)
   if (SONAR_DIST[i]>-1&&SONAR_DIST[i]<dmin) dmin=SONAR_DIST[i];
 }
 if (dmin<DistLimit)   // Too close to a surface in the horizontal direction
 {
  if (Angle()>1||Angle()>359)
  {
   if (Angle()>=180) Rotate(360-Angle());
   else Rotate(-Angle());
   return;
  }
  if (Velocity_Y()>2.0){
   Main_Thruster(0.0);
  }
  else
  {
   Main_Thruster(1.0);
  }
 }
}
