/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "Lee.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace phaseChangeTwoPhaseMixtures
{
    defineTypeNameAndDebug(Lee, 0);
    addToRunTimeSelectionTable(phaseChangeTwoPhaseMixture, Lee, components);
}
}

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::phaseChangeTwoPhaseMixtures::Lee::Lee
(
    const volVectorField& U,
    const surfaceScalarField& phi
)
:
    phaseChangeTwoPhaseMixture(typeName, U, phi),

    Cc_("Cc", phaseChangeTwoPhaseMixtureCoeffs_.subDict(type() + "Coeffs")),
    Cv_("Cv", phaseChangeTwoPhaseMixtureCoeffs_.subDict(type() + "Coeffs")),

    mcCoeff_(Cc_*rho2()),
    mvCoeff_(Cv_*rho1())
{
	Info<< "Phase change relaxation time factors for the " << type() << " model:\n" 
		<< "Cc = " << Cc_ << endl
		<< "Cv = " << Cv_ << endl;
}


// * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * * //

Foam::Pair<Foam::tmp<Foam::volScalarField> >
Foam::phaseChangeTwoPhaseMixtures::Lee::mDotAlphal() const
{
	return Pair<tmp<volScalarField>>
	(
		tmp<volScalarField>(mCondNoAlphal_),
		tmp<volScalarField>(mEvapNoAlphal_)
    );
}

Foam::Pair<Foam::tmp<Foam::volScalarField> >
Foam::phaseChangeTwoPhaseMixtures::Lee::mDotP() const
{
	return Pair<tmp<volScalarField> >
	(
	 // implicit implementation results in zero velocity at the interface
        //mCondAlphal_*pos(p_-pSat_)/max(p_-pSat_,1E-6*pSat_),
	    //mEvapAlphal_*neg(p_-pSat_)/max(pSat_-p_,1E-6*pSat_)
        -mCondAlphal_,
	    mEvapAlphal_
	);
}

Foam::Pair<Foam::tmp<Foam::volScalarField> >
Foam::phaseChangeTwoPhaseMixtures::Lee::mDotT() const
{
	return Pair<tmp<volScalarField> >
	(
	    tmp<volScalarField>(mCondNoTmTSat_),
	    tmp<volScalarField>(mEvapNoTmTSat_)
	);
}

void Foam::phaseChangeTwoPhaseMixtures::Lee::correct()
{
	phaseChangeTwoPhaseMixture::correct();

	calcLimitedAlphal();

	// minus sign "-" to provide mc > 0  and mv < 0
	// plus sign to provide mc < 0  and mv > 0
	if(cond_)
	{
		mCondNoAlphal_ = -mcCoeff_*neg(T_ - TSat_)*(T_ - TSat_)/TSat_;
		mCondAlphal_   = mCondNoAlphal_*(scalar(1) - limitedAlphal_);
		mCondNoTmTSat_ = -mcCoeff_*neg(T_ - TSat_)*(scalar(1) - limitedAlphal_)/TSat_;
	}

	if(evap_)
	{
		mEvapNoAlphal_ = -mvCoeff_*pos(T_ - TSat_)*(T_ - TSat_)/TSat_;
		mEvapAlphal_   = mEvapNoAlphal_*limitedAlphal_;
		mEvapNoTmTSat_ =  mvCoeff_*pos(T_ - TSat_)*limitedAlphal_/TSat_;
	}
}

bool Foam::phaseChangeTwoPhaseMixtures::Lee::read()
{
    if (phaseChangeTwoPhaseMixture::read())
    {
        phaseChangeTwoPhaseMixtureCoeffs_ = subDict(type() + "Coeffs");

        phaseChangeTwoPhaseMixtureCoeffs_.lookup("Cc") >> Cc_;
        phaseChangeTwoPhaseMixtureCoeffs_.lookup("Cv") >> Cv_;

        mcCoeff_ = Cc_*rho2();
        mvCoeff_ = Cv_*rho1();

        return true;
    }
    else
    {
        return false;
    }
}


// ************************************************************************* //
