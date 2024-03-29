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

#include "ChoiZhang.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace phaseChangeTwoPhaseMixtures
{
    defineTypeNameAndDebug(ChoiZhang, 0);
    addToRunTimeSelectionTable(phaseChangeTwoPhaseMixture, ChoiZhang, components);
}
}

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::phaseChangeTwoPhaseMixtures::ChoiZhang::ChoiZhang
(
    const volVectorField& U,
    const surfaceScalarField& phi
)
:
    Lee(U, phi),

    Ccvar_(Cc_),
	totvmCond_(gSum(mCondAlphal_)),
	totvmEvap_(gSum(mEvapAlphal_))
{
	Info<< "Phase change relaxation time factors for the " << type() << " model:\n" 
		<< "Cc = " << Ccvar_ << endl
		<< "Cv = " << Cv_ << endl;
}


// * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * * //


void Foam::phaseChangeTwoPhaseMixtures::ChoiZhang::correct()
{
	phaseChangeTwoPhaseMixtures::Lee::correct();

	//Ccvar_ = Cc_; // jak tego nie ma to Cc najpierw maleje do 1e-150 apotem rosnie do 1e+300
	totvmCond_ = gSum(mCondAlphal_);
	totvmEvap_ = gSum(mEvapAlphal_);
	
	// model makes sense only if both condensation and evaporation appear
	if (cond_ && evap_)
	{
		if (!(totvmCond_.value() == 0 && mag(totvmEvap_).value() == 0))
			Ccvar_ -= Ccvar_*((totvmCond_ - mag(totvmEvap_))
		    	/max(totvmCond_, mag(totvmEvap_)));
	}
	else
	{
		Ccvar_ = Cc_;
	}
	
    mcCoeff_ = Ccvar_*rho2();
	//Info<< "Updated mcCoeff = " << mcCoeff_.value() << endl;

}


// ************************************************************************* //
