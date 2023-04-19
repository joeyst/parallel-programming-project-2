
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

float x = Ranf( -1.f, 1.f );

float Ranf( unsigned int *seed, float low, float high )
{
        float r = (float) rand( );              // 0 - RAND_MAX

        return(   low  +  r * ( high - low ) / (float)RAND_MAX   );
}

int	NowYear = 0;		// 2023 - 2028
int	NowMonth = 2023;		// 0 - 11

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


void update_temp_and_precip() {
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

/*
SIMULATION FUNCTIONS 
*/

void RepeatSimulation(void (*SimFn) (void)) {
	while (NowYear < 2019) {
		while (NowMonth < 12) {
			NowMonth++;
			SimFn();
		}
		NowMonth = 0;
	}
}

void FixMe() {
	throw "FixMe.";
}

template <class ReturnType>
void Simulate(ReturnType (*Compute) (void), void (*Assign) (void), void (*Print) (void)) {
	// Compute 
	Compute();
	WaitBarrier();

	// Assign 
	Assign();
	WaitBarrier();

	// Print
	Print();
	WaitBarrier();
}

void Rabbits() {
	Simulate(GetRabbitQuantity, FixMe, NULL);
}

void RyeGrass() {
	Simulate(GetRyeGrassQuantity, FixMe, NULL);
}

void Watcher() {
	Simulate<void>(NULL, NULL, FixMe);
}

void Mutation() {

	WaitBarrier();

	WaitBarrier();

	WaitBarrier();
}

void A() {
	while( NowYear < 2029 )
		{
			// compute a temporary next-value for this quantity
			// based on the current state of the simulation:
			. . .

			// DoneComputing barrier:
			WaitBarrier( );	-- or --   #pragma omp barrier;
			. . .

			// DoneAssigning barrier:
			WaitBarrier( );	-- or --   #pragma omp barrier;
			. . .

			// DonePrinting barrier:
			WaitBarrier( );	-- or --   #pragma omp barrier;
			. . .
		}
}

omp_set_num_threads( 4 );	// same as # of sections
#pragma omp parallel sections
{
	#pragma omp section
	{
		Rabbits( );
	}

	#pragma omp section
	{
		RyeGrass( );
	}

	#pragma omp section
	{
		Watcher( );
	}

	#pragma omp section
	{
		MyAgent( );	// your own
	}
}       // implied barrier -- all functions must return in order
	// to allow any of them to get past here