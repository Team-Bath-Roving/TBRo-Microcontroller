#include <Arduino.h>

// set orientation of barrels (true if screws turn inwards to drive forward)
// (inwards meaning clockwise on left, anticlockwise on right)
#define THREADS_INWARDS true

enum microstep{
	FULL      = 1,
	HALF      = 2,
	QUARTER   = 4,
	EIGHTH    = 8,
	SIXTEENTH = 16
};

enum commandType{
	FORWARD   = 'W', // drive forward
	BACKWARD  = 'S', // drive back
	LEFT      = 'A', // roll left
	RIGHT     = 'D', // roll right
	PIVOT_R   = 'E', // pivot to the right while stationary
	PIVOT_L   = 'Q', // pivot to the left while stationary
	TURN_R    = 'L', // turn left while moving
	TURN_L    = 'J', // turn right while moving
	L_FORWARD = '[', // set left side speed (tank controls)
	R_FORWARD = ']', // set right side speed (tank controls)
	STOP      = 'X', // any unrecognised command will stop
	SET_SPEED = ':', // change max speed
	SET_ACCEL = '@', // change accel
	SET_MICROSTEP = 'M' // change microstep value
};

uint16_t stepCount=0;

class continuousStepper{
	int16_t speed=0;
	uint16_t accel=50;
	int16_t targetSpeed=0;
	unsigned long prevStepTime_us=0;
	unsigned long prevAccelTime_ms=0;
	int16_t maxSpeed=200;
	microstep stepsize;
	bool invert;
	bool stepState=0;
	uint8_t EN,DIR,STP,SLP,RST,MS1,MS2,MS3;

public:
	continuousStepper(	uint8_t dir,
						uint8_t stp,
						uint8_t slp,
						uint8_t rst,
						uint8_t ms3,
						uint8_t ms2,
						uint8_t ms1,
						uint8_t en, 
						bool Invert=false) : DIR(dir) ,STP(stp) ,SLP(slp) ,RST(rst), MS3(ms3), MS2(ms2), MS1(ms1), EN(en), invert(Invert){};
	

	void init(microstep stepSize=FULL) {
		// set up pins
		pinMode(EN,OUTPUT);
		pinMode(DIR,OUTPUT);
		pinMode(STP,OUTPUT);
		pinMode(SLP,OUTPUT);
		pinMode(RST,OUTPUT);
		pinMode(MS1,OUTPUT);
		pinMode(MS2,OUTPUT);
		pinMode(MS3,OUTPUT);
		// disable driver
		off();
		// set microstepping
		stepsize=stepSize;
		switch (stepSize) {
		case FULL: 
			digitalWrite(MS1,LOW);
			digitalWrite(MS2,LOW);
			digitalWrite(MS3,LOW); break;
		case HALF: 
			digitalWrite(MS1,HIGH);
			digitalWrite(MS2,LOW);
			digitalWrite(MS3,LOW); break;
		case QUARTER: 
			digitalWrite(MS1,LOW);
			digitalWrite(MS2,HIGH);
			digitalWrite(MS3,LOW); break;
		case EIGHTH: 
			digitalWrite(MS1,HIGH);
			digitalWrite(MS2,HIGH);
			digitalWrite(MS3,LOW); break;
		case SIXTEENTH: 
			digitalWrite(MS1,HIGH);
			digitalWrite(MS2,HIGH);
			digitalWrite(MS3,HIGH); break;
		default: 
			digitalWrite(MS1,LOW);
			digitalWrite(MS2,LOW);
			digitalWrite(MS3,LOW);
		}
	}

	void on() {
		digitalWrite(SLP,HIGH); // disable sleep
		digitalWrite(RST,HIGH); // disable reset
		digitalWrite(EN,LOW); // enable driver
	}

	void off() {
		stop();
		speed=0;
		digitalWrite(SLP,LOW); // disable sleep
		digitalWrite(RST,LOW); // disable reset
		digitalWrite(EN,HIGH); // enable driver
	}

	void stop() {targetSpeed=0;};

	// accel in step/s^2
	void setAccel(uint16_t Accel) {
		accel=Accel;
	}
	void setMaxSpeed(int16_t stepsPerSecond){
		maxSpeed=stepsPerSecond;
	}
	void setSpeed(int16_t Speed) {
		constrain(Speed,-255,255);
		targetSpeed=map(Speed,-255,255,-maxSpeed*stepsize,maxSpeed*stepsize);
		// targetSpeed=((float)(maxSpeed*stepsize)/255)*Speed;
		// Serial.println(targetSpeed);
	};
	int16_t getSpeed() {return speed;};
	int16_t getTargetSpeed() {return targetSpeed;};
	void run() {
		// calculate new speed using accel parameter
		if (millis()-prevAccelTime_ms>1000/accel) {
			prevAccelTime_ms=millis();
			if (speed<targetSpeed) {
				speed+=stepsize;
				if (speed>targetSpeed) {
					speed=targetSpeed;
				}
			}
			else if (speed>targetSpeed) {
				speed-=stepsize;
				if (speed<targetSpeed) {
					speed=targetSpeed;
				}
			}
			// set direction based on speed
			if (speed>0) {
				if (invert) {
					digitalWrite(DIR,HIGH);
				} else {
					digitalWrite(DIR,LOW);
				}
			} else if (speed<0) {
				if (invert) {
					digitalWrite(DIR,LOW);
				} else {
					digitalWrite(DIR,HIGH);
				}
			}
		}
		

		
		// only toggle step pin after every half period at the set speed
		if (speed!=0) {
			if (micros()-prevStepTime_us>(unsigned long)1000000/(abs(speed)*2)) {
				prevStepTime_us=micros();
				stepState=!stepState;
				digitalWrite(STP,stepState);
				stepCount++;
			}
		}

		// power saving
		if (speed==0 && targetSpeed==0) {
			off();
		} else {
			on();
		}
	};

};

// Invert motors such that positive speed turns inwards (clockwise on left, anticlockwise on right)
continuousStepper leftMotor(A0,A1,A2,A3,A4,A5,A6,A7,false);
continuousStepper rightMotor(2,3,4,5,6,7,8,9,true);

void setup() {
	Serial.begin(115200);
	pinMode(13,INPUT); // used to detect power to motors, prevent sudden current
	leftMotor.init(QUARTER);
	leftMotor.on();
	rightMotor.init(QUARTER);
	rightMotor.on();
}

int16_t speedL=0;
int16_t speedR=0;
int16_t offsetL=0;
int16_t offsetR=0;

void receiveCommands () {
	if (Serial.available()) {
		String command = Serial.readStringUntil('\n');
		command.toUpperCase();
		byte type=(byte)command[0];
		// Serial.println(type);
		String val = command.substring(1,command.length());
		int16_t value=val.toInt();
		constrain(value,-255,255);
		switch(type) {
		case FORWARD: // rotate inwards to drive forward
			Serial.print("FORWARD ");
			speedL=value;
			speedR=value; break;
		case BACKWARD: // rotate outwards to drive backward
			Serial.print("BACKWARD ");
			speedL=-value;
			speedR=-value; break;
		case LEFT: // turn both anticlockwise to roll left
			Serial.print("LEFT ");
			speedL=-value;
			speedR=value; break;
		case RIGHT: // turn both clockwise to roll right
			Serial.print("RIGHT ");
			speedL=value;
			speedR=-value; break;
		case PIVOT_R: // turn by reducing speed on right side 
			Serial.print("PIVOT_R ");
			speedR=-value; 
			speedL=0;break;
		case PIVOT_L: // turn by reducing speed on left side 
			Serial.print("PIVOT_L ");
			speedL=-value; 
			speedR=0; break;
		case TURN_R: // turn by reducing speed on right side 
			Serial.print("TURN_R ");
			offsetR=-value;
			break;
		case TURN_L: // turn by reducing speed on left side 
			Serial.print("TURN_L ");
			offsetL=-value; break;
		case L_FORWARD: // set speed of left side
			Serial.print("L_FORWARD ");
			speedL=value; break;
		case R_FORWARD: // set speed of right side
			Serial.print("R_FORWARD ");
			speedR=value; break;
		case SET_SPEED: // set speed of right side
			Serial.print("SET_SPEED ");
			leftMotor.setSpeed(value);
			rightMotor.setSpeed(value); break;
		case SET_ACCEL: // set accel of right side
			Serial.print("SET_ACCEL ");
			leftMotor.setAccel(value);
			rightMotor.setAccel(value); break;
		case SET_MICROSTEP: // set speed of right side
			Serial.print("SET_MICROSTEP ");
			leftMotor.setSpeed(value);
			rightMotor.setSpeed(value); break;

		default:
			Serial.print("STOP ");
			speedL=0;
			speedR=0;
			offsetL=0;
			offsetR=0;
			leftMotor.stop();
			rightMotor.stop();
		}
		// Apply speed with offsets
		leftMotor.setSpeed(speedL+offsetL);
		rightMotor.setSpeed(speedR+offsetR);

		Serial.println(value);
	}
}
long prevPrint=0;
long prevSec=0;

void loop() {
	if (digitalRead(13)==0) { // if no power available, stop motors and set speed to zero
		speedL=0;
		speedR=0;
		offsetL=0;
		offsetR=0;
		leftMotor.off();
		rightMotor.off();
	} 
	receiveCommands();
	leftMotor.run();
	rightMotor.run();
	if (millis()-prevPrint>1000) {
		prevPrint=millis();
		// Serial.print(">leftMotor:");Serial.println(leftMotor.getSpeed());
		// Serial.print(">rightMotor:");Serial.println(rightMotor.getSpeed());
		// Serial.print(">leftMotorTarget:");Serial.println(leftMotor.getTargetSpeed());
		// Serial.print(">rightMotorTarget:");Serial.println(rightMotor.getTargetSpeed());
	}

	if (millis()-prevSec>1000) {
		prevSec=millis();
		// Serial.print(">stepFreq:");Serial.println((float)stepCount/2);
		stepCount=0;
	}

}