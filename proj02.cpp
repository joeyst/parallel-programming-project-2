
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

float Ranf( unsigned int *seed, float low, float high )
{
        float r = (float) rand( );              // 0 - RAND_MAX

        return(   low  +  r * ( high - low ) / (float)RAND_MAX   );
}

int	NowYear = 2023;		// 2023 - 2028
int	NowMonth = 0;		// 0 - 11

float	NowPrecip;		// inches of rain per month
float	NowTemp;		// temperature this month
float	NowHeight = 5.;		// rye grass height in inches
int	NowNumRabbits = 1;		// number of rabbits in the current population

unsigned int seed = 0;

const float RYEGRASS_GROWS_PER_MONTH =		20.0;
const float ONE_RABBITS_EATS_PER_MONTH =	 1.0;

const float AVG_PRECIP_PER_MONTH =	       12.0;	// average
const float AMP_PRECIP_PER_MONTH =		4.0;	// plus or minus
const float RANDOM_PRECIP =			2.0;	// plus or minus noise

const float AVG_TEMP =				60.0;	// average
const float AMP_TEMP =				20.0;	// plus or minus
const float RANDOM_TEMP =			10.0;	// plus or minus noise

const float MIDTEMP =				60.0;
const float MIDPRECIP =				14.0;

// specify how many threads will be in the barrier:
//	(also init's the Lock)

omp_lock_t	Lock;
volatile int	NumInThreadTeam;
volatile int	NumAtBarrier;
volatile int	NumGone;

void
InitBarrier( int n )
{
	NumInThreadTeam = n;
	NumAtBarrier = 0;
	omp_init_lock( &Lock );
}


// have the calling thread wait here until all the other threads catch up:

void
WaitBarrier( )
{
	omp_set_lock( &Lock );
	{
		NumAtBarrier++;
		if( NumAtBarrier == NumInThreadTeam )
		{
			NumGone = 0;
			NumAtBarrier = 0;
			// let all other threads get back to what they were doing
			// before this one unlocks, knowing that they might immediately
			// call WaitBarrier( ) again:
			while( NumGone != NumInThreadTeam-1 );
			omp_unset_lock( &Lock );
			return;
		}
	}
	omp_unset_lock( &Lock );

	while( NumAtBarrier != 0 );	// this waits for the nth thread to arrive

	#pragma omp atomic
	NumGone++;			// this flags how many threads have returned
}


void UpdateTemperatureAndPrecipitation() {
	float ang = (  30.*(float)NowMonth + 15.  ) * ( M_PI / 180. );

	float temp = AVG_TEMP - AMP_TEMP * cos( ang );
	NowTemp = temp + Ranf( &seed, -RANDOM_TEMP, RANDOM_TEMP );

	float precip = AVG_PRECIP_PER_MONTH + AMP_PRECIP_PER_MONTH * sin( ang );
	NowPrecip = precip + Ranf( &seed,  -RANDOM_PRECIP, RANDOM_PRECIP );
	if( NowPrecip < 0. )
		NowPrecip = 0.;
}

int GetRabbitQuantity() {
	int nextNumRabbits = NowNumRabbits;
	int carryingCapacity = (int)( NowHeight );
	if( nextNumRabbits < carryingCapacity ) {
		nextNumRabbits++;
	}
	else if( nextNumRabbits > carryingCapacity ) {
		nextNumRabbits--;
	}

	if( nextNumRabbits < 0 ) {
		nextNumRabbits = 0;
	}

	return nextNumRabbits;
}

void SetRabbitQuantity(int NewRabbitQuantity) {
	NowNumRabbits = NewRabbitQuantity;
}

void SetRyeGrassQuantity(float NewRyeGrassQuantity) {
	NowHeight = NewRyeGrassQuantity;
}

float
Sqr( float x )
{
        return x*x;
}

float GetRyeGrassQuantity() {
	float tempFactor = exp(   -Sqr(  ( NowTemp - MIDTEMP ) / 10.  )   );
	float precipFactor = exp(   -Sqr(  ( NowPrecip - MIDPRECIP ) / 10.  )   );

	float nextHeight = NowHeight;
	nextHeight += tempFactor * precipFactor * RYEGRASS_GROWS_PER_MONTH;
	nextHeight -= (float)NowNumRabbits * ONE_RABBITS_EATS_PER_MONTH;

	return nextHeight;
}

int PlaceholderReturn(void) {
	return 0;
}

void PlaceholderAssign(int) {

}

/*
SIMULATION FUNCTIONS 
*/

void RepeatSimulation(void (*SimFn) (void)) {
	while (NowYear < 2029) SimFn();
}

void FixMe() {
	throw "FixMe.";
}

void DoNothing(void) {
	
}

template <class VariableType>
void Simulate(VariableType (*Compute) (void), void (*Assign) (VariableType), void (*Print) (void)) {
	// Compute 
	VariableType Value = Compute();
	WaitBarrier();

	// Assign 
	Assign(Value);
	WaitBarrier();

	// Print
	Print();
	WaitBarrier();
}

void Print() {
	UpdateTemperatureAndPrecipitation();	
	printf("%i, %i, %f, %f, %f, %i\n", NowYear, NowMonth, NowPrecip, NowTemp, NowHeight, NowNumRabbits);
	NowMonth++;
	if (NowMonth % 12 == 0) {
		NowYear++;
	}
}

void Rabbits() {
	Simulate(GetRabbitQuantity, SetRabbitQuantity, DoNothing);
}

void RyeGrass() {
	Simulate(GetRyeGrassQuantity, SetRyeGrassQuantity, DoNothing);
}

void Watcher() {
	Simulate(PlaceholderReturn, PlaceholderAssign, Print);
}

// void Mutation() {
// 	Simulate(FixMe, FixMe, DoNothing);
// }

int main() {
	UpdateTemperatureAndPrecipitation();

	omp_set_num_threads( 3 );	// same as # of sections
	InitBarrier( 3 );
	#pragma omp parallel sections
	{
		#pragma omp section
		{
			RepeatSimulation(Rabbits);
		}

		#pragma omp section
		{
			RepeatSimulation(RyeGrass);
		}

		#pragma omp section
		{
			RepeatSimulation(Watcher);
		}

		// #pragma omp section
		// {
		// 	Mutation( );
		// }
	}
}
