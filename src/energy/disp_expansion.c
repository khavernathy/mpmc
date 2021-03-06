#include <mc.h>

//Copyright 2013-2015 Adam Hogan

double disp_expansion_lrc( const system_t * system,  pair_t * pair_ptr, const double cutoff ) /* ignoring the exponential repulsion bit because it decays exponentially */
{
	if( !( pair_ptr->frozen ) &&  /* disqualify frozen pairs */
		((pair_ptr->lrc == 0.0) || pair_ptr->last_volume != system->pbc->volume) ) { /* LRC only changes if the volume change */

		pair_ptr->last_volume = system->pbc->volume;

		return -4.0*M_PI*(pair_ptr->c6/(3.0*cutoff*cutoff*cutoff)+pair_ptr->c8/(5.0*cutoff*cutoff*cutoff*cutoff*cutoff)+pair_ptr->c10/(7.0*cutoff*cutoff*cutoff*cutoff*cutoff*cutoff*cutoff))/system->pbc->volume;
	}

	else return pair_ptr->lrc; /* use stored value */
}

double disp_expansion_lrc_self( const system_t * system, atom_t * atom_ptr, const double cutoff )
{
	if( !(atom_ptr->frozen) && /* disqualify frozen atoms */
 		((atom_ptr->lrc_self == 0.0) || atom_ptr->last_volume != system->pbc->volume) ) { /* LRC only changes if the volume change) */

		atom_ptr->last_volume = system->pbc->volume;

		if ( system->extrapolate_disp_coeffs)
		{
			double c10;
			if (atom_ptr->c6!=0.0&&atom_ptr->c8!=0.0)
				c10 = 49.0/40.0*atom_ptr->c8*atom_ptr->c8/atom_ptr->c6;
			else
				c10 = 0.0;

			return -4.0*M_PI*(atom_ptr->c6/(3.0*cutoff*cutoff*cutoff)+atom_ptr->c8/(5.0*cutoff*cutoff*cutoff*cutoff*cutoff)+c10/(7.0*cutoff*cutoff*cutoff*cutoff*cutoff*cutoff*cutoff))/system->pbc->volume;
		}
		else
			return -4.0*M_PI*(atom_ptr->c6/(3.0*cutoff*cutoff*cutoff)+atom_ptr->c8/(5.0*cutoff*cutoff*cutoff*cutoff*cutoff)+atom_ptr->c10/(7.0*cutoff*cutoff*cutoff*cutoff*cutoff*cutoff*cutoff))/system->pbc->volume;
	}

	return atom_ptr->lrc_self; /* use stored value */
}

double disp_expansion(system_t *system)
{
	double potential = 0.0;

	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr;

	for(molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {
			for(pair_ptr = atom_ptr->pairs; pair_ptr; pair_ptr = pair_ptr->next) {

				if(pair_ptr->recalculate_energy) {

					/* pair LRC */
					if ( system->rd_lrc )
						pair_ptr->lrc = disp_expansion_lrc(system,pair_ptr,system->pbc->cutoff);

					/* make sure we're not excluded or beyond the cutoff */
					if(!(pair_ptr->rd_excluded || pair_ptr->frozen)) {
						const double r = pair_ptr->rimg;
						const double r2 = r*r;
						const double r4 = r2*r2;
						const double r6 = r4*r2;
						const double r8 = r6*r2;
						const double r10 = r8*r2;

						double c6 = pair_ptr->c6;
						const double c8 = pair_ptr->c8;
						const double c10 = pair_ptr->c10;

						if (system->disp_expansion_mbvdw==1)
							c6 = 0.0;

						double repulsion = 0.0;

						if (pair_ptr->epsilon!=0.0&&pair_ptr->sigma!=0.0)
							repulsion = 315.7750382111558307123944638 * exp(-pair_ptr->epsilon*(r-pair_ptr->sigma)); // K = 10^-3 H ~= 316 K

						if (system->damp_dispersion)
							pair_ptr->rd_energy = -tt_damping(6,pair_ptr->epsilon*r)*c6/r6-tt_damping(8,pair_ptr->epsilon*r)*c8/r8-tt_damping(10,pair_ptr->epsilon*r)*c10/r10+repulsion;
						else
							pair_ptr->rd_energy = -c6/r6-c8/r8-c10/r10+repulsion;

						if(system->cavity_autoreject)
						{
							if(r < system->cavity_autoreject_scale*pair_ptr->sigma)
								pair_ptr->rd_energy = MAXVALUE;
							if(system->cavity_autoreject_repulsion!=0.0&&repulsion>system->cavity_autoreject_repulsion)
								pair_ptr->rd_energy = MAXVALUE;
						}
					}

				}
				potential += pair_ptr->rd_energy + pair_ptr->lrc;
			}
		}
	}

	if (system->disp_expansion_mbvdw==1)
	{
		thole_amatrix(system);
		potential += vdw(system);
	}

	/* calculate self LRC interaction */
	if ( system->rd_lrc )
	{
		for(molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next)
		{
			for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next)
			{
				atom_ptr->lrc_self = disp_expansion_lrc_self(system,atom_ptr,system->pbc->cutoff);
				potential += atom_ptr->lrc_self;
			}
		}
	}

	return potential;
}

double disp_expansion_nopbc(system_t *system)
{
	double potential = 0.0;

	molecule_t *molecule_ptr;
	atom_t *atom_ptr;
	pair_t *pair_ptr;

	for(molecule_ptr = system->molecules; molecule_ptr; molecule_ptr = molecule_ptr->next) {
		for(atom_ptr = molecule_ptr->atoms; atom_ptr; atom_ptr = atom_ptr->next) {
			for(pair_ptr = atom_ptr->pairs; pair_ptr; pair_ptr = pair_ptr->next) {

				if(pair_ptr->recalculate_energy) {
					/* make sure we're not excluded or beyond the cutoff */
					if(!(pair_ptr->rd_excluded || pair_ptr->frozen)) {
						const double r = pair_ptr->rimg;
						const double r2 = r*r;
						const double r4 = r2*r2;
						const double r6 = r4*r2;
						const double r8 = r6*r2;
						const double r10 = r8*r2;

						double c6 = pair_ptr->c6;
						const double c8 = pair_ptr->c8;
						const double c10 = pair_ptr->c10;

						if (system->disp_expansion_mbvdw==1)
							c6 = 0.0;

						double repulsion = 0.0;

						if (pair_ptr->epsilon!=0.0&&pair_ptr->sigma!=0.0)
							repulsion = 315.7750382111558307123944638 * exp(-pair_ptr->epsilon*(r-pair_ptr->sigma)); // K = 10^-3 H ~= 316 K

						if (system->damp_dispersion)
							pair_ptr->rd_energy = -tt_damping(6,pair_ptr->epsilon*r)*c6/r6-tt_damping(8,pair_ptr->epsilon*r)*c8/r8-tt_damping(10,pair_ptr->epsilon*r)*c10/r10+repulsion;
						else
							pair_ptr->rd_energy = -c6/r6-c8/r8-c10/r10+repulsion;

						if(system->cavity_autoreject)
						{
							if(r < system->cavity_autoreject_scale*pair_ptr->sigma)
								pair_ptr->rd_energy = MAXVALUE;
							if(system->cavity_autoreject_repulsion!=0.0&&repulsion>system->cavity_autoreject_repulsion)
								pair_ptr->rd_energy = MAXVALUE;
						}
					}

				}
				potential += pair_ptr->rd_energy;
			}
		}
	}

	if (system->disp_expansion_mbvdw==1)
	{
		thole_amatrix(system);
		potential += vdw(system);
	}

	return potential;
}

double factorial(int n)
{
	int i;
	double fac = 1.0;
	for (i=2;i<=n;i++)
		fac *= i;
	return fac;
}

double tt_damping(int n, double br)
{
	double sum = 0.0;
	int i;
	for (i=0;i<=n;i++)
	{
		sum += pow(br,i)/factorial(i);
	}

	const double result = 1.0-exp(-br)*sum;

	if (result>0.000000001)
		return result;
	else
		return 0.0; /* This is so close to zero lets just call it zero to avoid rounding error and the simulation blowing up */
}
