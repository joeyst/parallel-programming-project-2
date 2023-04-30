
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

/*
Compiled to `proj02` executable with: 
`g++ proj02.cpp -o proj02 -lm -fopenmp` 
with variables: 
`DINITIALHEIGHT`
`DNOWNUMRABBITS`
`DRYEGRASSGROWSPERMONTH`
`DONERABBITSEATSPERMONTH`
`DONEMUTANTSEATSPERMONTH`
E.g.: `g++ proj02.cpp -o proj02 -lm -fopenmp -DINITIALHEIGHT=420`
*/

float Ranf( unsigned int *seed, float low, float high )
{
        float r = (float) rand( );              // 0 - RAND_MAX

        return(   low  +  r * ( high - low ) / (float)RAND_MAX   );
}

int	NowYear = 2023;		// 2023 - 2028
int	NowMonth = 0;		// 0 - 11

float	NowPrecip;		// inches of rain per month
float	NowTemp;		// temperature this month

/*
Defines here so that can run `script.bash` with custom "arguments". 
*/
#ifndef INITIALHEIGHT
#define INITIALHEIGHT 5.
#endif

#ifndef NOWNUMRABBITS 
#define NOWNUMRABBITS 5
#endif

#ifndef RYEGRASSGROWSPERMONTH 
#define RYEGRASSGROWSPERMONTH 20.0 
#endif 

#ifndef ONERABBITSEATSPERMONTH 
#define ONERABBITSEATSPERMONTH 1.0
#endif

#ifndef ONEMUTANTSEATSPERMONTH
#define ONEMUTANTSEATSPERMONTH 2.0
#endif

float	NowHeight = INITIALHEIGHT;		// rye grass height in inches
int	NowNumRabbits = NOWNUMRABBITS;		// number of rabbits in the current population
int NowNumMutants = 0;

unsigned int seed = 0;

const float RYEGRASS_GROWS_PER_MONTH =		RYEGRASSGROWSPERMONTH;
const float ONE_RABBITS_EATS_PER_MONTH =	 ONERABBITSEATSPERMONTH;
const float ONE_MUTANTS_EATS_PER_MONTH = ONEMUTANTSEATSPERMONTH;

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
	nextHeight -= (float)NowNumMutants * ONE_MUTANTS_EATS_PER_MONTH;

	if (nextHeight < 0.) nextHeight = 0.;

	return nextHeight;
}

int GetMutationQuantity() {
	/*
	Mutants have a 25% chance of breeding from mutants and regular 
	rabbits. 

	They have a 10% chance of breeding from regular rabbits and regular rabbits.

	Mutant rabbits always breed with regular rabbits. 

	Unfortunately, mutant rabbits die after a month, guaranteed.  
	*/

	/*
	If the number of mutants is less than the number of rabbits, then subtract 
	the number of mutants from the number of rabbits and calculate the new number of 
	rabbits and regular rabbits. 
	*/

	/*
	If there are fewer mutants than non-mutants, 25% chance for a mutant 
	for each (mutant, non-mutant) pair there is. 10% chance for (non-mutant, 
	non-mutant) pairs. 
	*/
	if (NowNumMutants < NowNumRabbits) {
		int LeftoverNumRabbits = NowNumRabbits - NowNumMutants;
		int NewNumMutants = 0;
		float AttemptValue;
		for (int i = 0; i < NowNumMutants; i++) {
			AttemptValue = Ranf( &seed, 0., 1. );
			if (AttemptValue < 0.25) NewNumMutants++;
		}

		for (int i = 0; i < LeftoverNumRabbits; i++) {
			AttemptValue = Ranf( &seed, 0., 1. );
			if (AttemptValue < 0.10) NewNumMutants++;
		}
		return NewNumMutants;
	}

	/*
	If the number of mutants is greater than or equal to the number of rabbits, 
	we'll have a (mutant, non-mutant) pair for every single non-mutant. 
	*/
	else {
		int NewNumMutants = 0;
		float AttemptValue;
		for (int i = 0; i < NowNumMutants; i++) {
			AttemptValue = Ranf( &seed, 0., 1. );
			if (AttemptValue < 0.25) NewNumMutants++;
		}
		return NewNumMutants;
	}
}

void SetMutationQuantity(int Quantity) {
	NowNumMutants = Quantity;
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
	printf("%-8i, %-8i, %-10.4f, %-10.4f, %-10.4f, %-8i, %-8i\n", NowYear, NowMonth, NowPrecip, NowTemp, NowHeight, NowNumRabbits, NowNumMutants);
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

void Mutation() {
	Simulate(GetMutationQuantity, SetMutationQuantity, DoNothing);
}

int main() {
	UpdateTemperatureAndPrecipitation();

	printf("NowYear, NowMonth, NowPrecip, NowTemp, NowHeight, NowNumRabbits, NowNumMutants\n");
	omp_set_num_threads( 4 );	// same as # of sections
	InitBarrier( 4 );
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

		#pragma omp section
		{
			RepeatSimulation(Mutation);
		}
	}
}
