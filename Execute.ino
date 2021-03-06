/**
 * Main switch case for the serial input. Uses a if-else so that full strings 
 * can be used as the command, but it could be changed to a switch-case if 
 * needed. The only way switch-case is better is performance, but that's not 
 * necessarily an issue here.
 */
void execute(String command, String parameter){

	if (command.compareTo("move") == 0){
		parseMove(parameter);
	}
	else if (command.compareTo("moveSteps") == 0){
		parseMoveSteps(parameter);
	}
	else if (command.compareTo("nozzleHeight") == 0){
		setNozzleHeight(parameter);
	}
	else if (command.compareTo("calibrate") == 0){
		calibrate();
	}
	else if (command.compareTo("dispense") == 0){
		parseDispense(parameter);
	}
	else{
		Serial.println("Didn't recognize the command: " + command + ":" + parameter);
	}
}

/**
 * Parse the input move parameter, then call moveMotor to actually do the movement.
 *
 * @param parameter - movement, in mm, relative to current position, in the form of parameter = x,y
 */
void parseMove(String parameter){
	//separate the x from y values in this parameter
	int delimPos = parameter.indexOf(',');
	String xString = parameter.substring(0, delimPos);
	String yString = parameter.substring(delimPos + 1);
	int xDecimalPos = xString.indexOf('.');
	int yDecimalPos = yString.indexOf('.');
	int xNumBeforeDecimal = xString.substring(0, xDecimalPos).toInt();
	int yNumBeforeDecimal = yString.substring(0, yDecimalPos).toInt();
	String xAfterDecimal = xString.substring(xDecimalPos + 1);
	String yAfterDecimal = yString.substring(yDecimalPos + 1);
	float xToMove = abs(xNumBeforeDecimal) + xAfterDecimal.toInt()/pow(10.0, xAfterDecimal.length());
	float yToMove = abs(yNumBeforeDecimal) + yAfterDecimal.toInt()/pow(10.0, yAfterDecimal.length());

	if (xNumBeforeDecimal < 0) {
		xToMove *= -1;
	}
	if (yNumBeforeDecimal < 0) {
		yToMove *= -1;
	}
	//int yToMove = yString.toCharArray("", yString.length());
	//int xToMove = atof((parameter.substring(0, delimPos)).toCharArray(char[], ));		//THIS IS IN MM
	//int yToMove = atof((parameter.substring(delimPos + 1)).toCharArray());	//THIS IS IN MM
	//int yToMove = (parameter.substring(delimPos + 1)).toFloat();	//THIS IS IN MM

	//and now call to move the motor that amount
	moveMillimeters(xToMove, yToMove);
}

/**
 * Parse the input move parameter, then call moveSteps to actually do the movement.
 *
 * @param parameter - movement, in steps, relative to current position, in the form of parameter = x,y
 */
void parseMoveSteps(String parameter){
	//separate the x from y values in this parameter
	int delimPos = parameter.indexOf(',');
	int xToMove = (parameter.substring(0, delimPos)).toInt();
	int yToMove = (parameter.substring(delimPos + 1)).toInt();

	//and now call to move the motor that amount
	moveSteps(xToMove, yToMove);
}

/**
 * Moves the arm to the relative location defined by the two input floats, which are in mms.
 */
void moveMillimeters(float xToMove, float yToMove){
	float mmToStepConversion = Y_BOUND/92;

	//convert the input cm's to steps for the motor
	int xSteps = round(xToMove*mmToStepConversion);
	int ySteps = round(yToMove*mmToStepConversion);

	moveSteps(xSteps, ySteps);
}

void moveSteps(int xSteps, int ySteps){
	//if the software is telling motors to move out of their allowed bounds, don't do it
	if (currentPosition[0] + xSteps > X_BOUND){
		Serial.println("Arm is going too far in x direction!");
	}
	else if (currentPosition[1] + ySteps > Y_BOUND){
		Serial.println("Arm is going too far in y direction!");
	}
	//actually set AccelStepper to move the motors, relative to the current position
	else{
		taskIsExecuting = 0;
		digitalWrite(xAxisEnable, LOW);
		digitalWrite(yAxisEnable, LOW);
		xMotor.move(xSteps);
		yMotor.move(ySteps);

		//update the failsafe internal state tracker
		currentPosition[0] += xSteps;
		currentPosition[1] += ySteps;
	}
}

/**
 * Rotates the nozzle servo to angle specified by parameter, which should be an int between 0 and 180.
 * Software should do all the processing regarding converting an input vertical distance parameter
 * to a value between 0 and 180 for the servo.
 *
 * TODO: later, we should probably be able to set whether the servo is normal or continuous rotation, and
 * do the setting of the height accordingly. right now, it assumes normal.
 *
 * @param parameter - value to feed the servo
 */
void setNozzleHeight(String parameter){
	int angleToSet = parameter.toInt();
	int timeToRun = timeForCompleteRotation*angleToSet/180;

	//normal servo operation, set the angle to go to and wait the appropriate amount of time
	if (SERVO_TYPE == "NORMAL"){
		nozzleServo.write(angleToSet, 60, true);
		//delay(timeToRun);
	}
	//continuous servo operation, just run it for the appropriate amount of time
	else if(SERVO_TYPE == "CONTINUOUS"){
		nozzleServo.writeMicroseconds(timeToRun);
	}
	//once the servo is done moving, send a message to the computer telling it this command is done, and it is
	//ready to receive another.
	Serial.println("Done");

}

/**
 * Calibrates the arm to (0,0), which means pushing it continuously to the bottom right and stopping
 * when calibrateX and calibrateY are both read to be high.
 */
void calibrate(){
	Serial.println("Calibrating!");
	//calibrate in y direction
	digitalWrite(xAxisEnable, LOW);
	digitalWrite(yAxisEnable, LOW);
	calibrateDirection(yMotor, calibrateY);
	//calibate in x direction
	calibrateDirection(xMotor, calibrateX);
	digitalWrite(xAxisEnable, HIGH);
	digitalWrite(yAxisEnable, HIGH);
	//update our failsafe tracker
	currentPosition[0] = 0;
	currentPosition[1] = 0;
}

/**
 * Used by calibrate, encapsulates calibrating in a certain direction.
 */
void calibrateDirection(AccelStepper motor, int motorPin){
	//move out of bounds
	
	motor.move(-9000);
	//while the calibrate pin is still low (we're still in bounds)
	while(digitalRead(motorPin) == LOW){
		//move it toward the bounds
		motor.run();
	}
	//but when we hit the bounds, stop moving
	motor.move(0);
	
}

/**
 * Move liquid with the pump. The input parameter specifies the number of steps to run the pump.
 * 
 */
void parseDispense(String parameter){
	int steps = parameter.toInt();
	//int delimPos = parameter.indexOf(',');
	//int fluidTime = (parameter.substring(0, delimPos)).toInt();
	//int airTime = (parameter.substring(delimPos + 1)).toInt();
	taskIsExecuting = 1;
	digitalWrite(pumpEnable, LOW);
	pumpMotor.move(steps);
}
