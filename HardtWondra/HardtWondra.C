/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | www.openfoam.com
     \\/     M anipulation  |
-------------------------------------------------------------------------------
    Copyright (C) 2022 AUTHOR,AFFILIATION
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

#include "HardtWondra.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

//const dataType Foam::HardtWondra::staticData();


// * * * * * * * * * * * * * * * Local Functions * * * * * * * * * * * * * * //


// * * * * * * * * * * * * * Static Member Functions * * * * * * * * * * * * //


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //
void Foam::HardtWondra::calcLimitedAlphal()
{
	limitedAlphal_ = min(max(alphalRef_, scalar(0)), scalar(1));
}

void Foam::HardtWondra::calcMagGradLimitedAlphal()
{
	calcLimitedAlphal();
	magGradLimitedAlphal_ = mag(fvc::grad(limitedAlphal_));
}


// * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * //


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::HardtWondra::HardtWondra
(
	const volScalarField& alpha1
)
:
	HWdict_
	(
	    IOdictionary
	    (
	        IOobject
            (
                "phaseChangeProperties", // dictionary name
                alpha1.time().constant(),     // dict is found in "constant"
                alpha1.db(),                  // registry for the dict
                IOobject::MUST_READ,     // must exist, otherwise failure
                IOobject::NO_WRITE       // dict is only read by the solver
            )
	    )
	),
    cond_{HWdict_.get<Switch>("condensation")},
    evap_{HWdict_.get<Switch>("evaporation")},
	alphalRef_{alpha1},
	cutoff_(HWdict_.getOrDefault<scalar>("cutoff", 1e-3)),
	spread_(HWdict_.getOrDefault<scalar>("spread", 3)),
	limitedAlphal_(min(max(alphalRef_, scalar(0)), scalar(1))),
	magGradLimitedAlphal_(mag(fvc::grad(limitedAlphal_)))
{
	Info<< "Condensation is   "   << cond_   << endl;
	Info<< "Evaporation is    "   << evap_   << endl;
	Info<< "Spread is set as: "   << spread_ << endl;
	Info<< "Cutoff is set as: "   << cutoff_ << endl;
}


// * * * * * * * * * * * * * * * * Selectors * * * * * * * * * * * * * * * * //

//Foam::autoPtr<Foam::HardtWondra>
//Foam::HardtWondra::New()
//{
//    return autoPtr<HardtWondra>(new HardtWondra);
//}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::HardtWondra::~HardtWondra()
{}


// * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * * //
void Foam::HardtWondra::spread()
{
	// 1) Calculate |grad(alphal)|
	calcMagGradLimitedAlphal();
	
	//- Smearing of source term field
	dimensionedScalar DPsi
	(
	    "DPsi",
	    dimArea,
	    spread_/sqr(gAverage(alphalRef_.mesh().nonOrthDeltaCoeffs()))
	);
	
	// 2) volume integral from |grad(alphal)|
	dimensionedScalar intPsi0 = fvc::domainIntegrate(magGradLimitedAlphal_);
	dimensionedScalar intAlphaPsi0 = fvc::domainIntegrate(limitedAlphal_*magGradLimitedAlphal_);
		
	// 4) calculate N
	dimensionedScalar N("N", dimensionSet(0,0,0,0,0,0,0), 2.0);
	if (intAlphaPsi0.value() > 1e-99)
	{
		N = intPsi0/intAlphaPsi0;
	}
	
	if (cond_)
	{
		// 5) phi0
		mCondAlphal_   *= N; // According to HW it should be like this
		//                       // but it gives slightly higher errors if uncommentd
		//mCondNoAlphal_ *= N;
		mCondNoTmTSat_ *= N;
	
		dimensionedScalar intPsi0l = fvc::domainIntegrate(mCondAlphal_);
	
		const fvMesh& mesh = alphalRef_.mesh();
		volScalarField psil
		(
		    IOobject
		    (
		        "psil",
		        mesh.time().timeName(),
		        mesh,
		        IOobject::NO_READ,
		        IOobject::NO_WRITE
		    ),
		    mesh,
		    dimensionedScalar(mCondAlphal_.dimensions(), Zero),
		    zeroGradientFvPatchField<scalar>::typeName
		);
		
		// 6) Solve Helmholtz equation
		fvScalarMatrix psilEqn
		(
			fvm::Sp(scalar(1),psil) - fvm::laplacian(DPsi,psil) == mCondAlphal_
		);
		
		psilEqn.solve();
		
		//- Cut cells with cutoff < alpha1 < 1-cutoff and rescale remaining source term field
		dimensionedScalar intPsiLiquidCondensation ("intPsiLiquidCondensation", dimensionSet(1,0,-1,0,0,0,0), 0.0);
		dimensionedScalar intPsiVaporCondensation ("intPsiVaporCondensation", dimensionSet(1,0,-1,0,0,0,0), 0.0);
	
	
		forAll(mesh.C(),iCell)
		{
			if ((limitedAlphal_[iCell]) < cutoff_)
			{
				intPsiVaporCondensation.value() += (1.0-limitedAlphal_[iCell])*psil[iCell]*mesh.V()[iCell];
			}
			else if ((limitedAlphal_[iCell]) > 1.0-cutoff_)
			{
				intPsiLiquidCondensation.value() += (limitedAlphal_[iCell])*psil[iCell]*mesh.V()[iCell];
			}
		}
	
		dimensionedScalar intPsil = fvc::domainIntegrate(psil);
		
		//- 7) Calculate Nl and Nv
		dimensionedScalar Nl ("Nl", dimensionSet(0,0,0,0,0,0,0), 2.0);
		dimensionedScalar Nv ("Nv", dimensionSet(0,0,0,0,0,0,0), 2.0);
		
		reduce(intPsiLiquidCondensation.value(),sumOp<scalar>());
		reduce(intPsiVaporCondensation.value(),sumOp<scalar>());
		
		if (intPsiLiquidCondensation.value() > 1e-99)
		{
		    Nl = intPsi0l/intPsiLiquidCondensation;
		}
		if (intPsiVaporCondensation.value() > 1e-99)
		{
		    Nv = intPsi0l/intPsiVaporCondensation;
		}
		
		        
		//- 8) Set source terms in cells with alpha1 < cutoff or alpha1 > 1-cutoff
		forAll(mesh.C(),iCell)
		{
			if (limitedAlphal_[iCell] < cutoff_)
			{
				mCondAlphal_[iCell] = -Nv.value()*(1.0-limitedAlphal_[iCell])*psil[iCell];
				//mCondNoTmTSat_[iCell] += -Nv.value()*(1.0-limitedAlphal_[iCell])*psil[iCell]*cp2_.value()*T_[iCell]/hEvap_.value();
			}
			else if (limitedAlphal_[iCell] > 1.0-cutoff_)
			{
				mCondAlphal_[iCell] = Nl.value()*(limitedAlphal_[iCell])*psil[iCell];
				//mCondNoTmTSat_[iCell] += Nl.value()*(limitedAlphal_[iCell])*psil[iCell]*cp1_.value()*T_[iCell]/hEvap_.value();
			}
			else
			{
				mCondAlphal_[iCell] = 0.0;
			}
		}
	}
	
	if (evap_)
	{
		//mEvapAlphal_   *= N; // According to HW it should be like this
		                       // but it gives slightly higher errors if uncommentd
		mEvapNoAlphal_ *= N;
		mEvapNoTmTSat_ *= N;
	
		dimensionedScalar intPsi0v = fvc::domainIntegrate(mEvapAlphal_);
	
		const fvMesh& mesh = T_.mesh();
		volScalarField psiv
		(
		    IOobject
		    (
		        "psiv",
		        mesh.time().timeName(),
		        mesh,
		        IOobject::NO_READ,
		        IOobject::NO_WRITE
		    ),
		    mesh,
		    dimensionedScalar(mEvapAlphal_.dimensions(), Zero),
		    zeroGradientFvPatchField<scalar>::typeName
		);
		
		fvScalarMatrix psivEqn
		(
			fvm::Sp(scalar(1),psiv) - fvm::laplacian(DPsi,psiv) == mEvapAlphal_
		);
		
		psivEqn.solve();
		
		//- Cut cells with cutoff < alpha1 < 1-cutoff and rescale remaining source term field
		dimensionedScalar intPsiLiquidEvaporation ("intPsiLiquidEvaporation", dimensionSet(1,0,-1,0,0,0,0), 0.0);
		dimensionedScalar intPsiVaporEvaporation ("intPsiVaporEvaporation", dimensionSet(1,0,-1,0,0,0,0), 0.0);
	
	
		forAll(mesh.C(),iCell)
		{
			if (limitedAlphal_[iCell] < cutoff_)
			{
				intPsiVaporEvaporation.value() += (1.0-limitedAlphal_[iCell])*psiv[iCell]*mesh.V()[iCell];
			}
			else if (limitedAlphal_[iCell] > 1.0-cutoff_)
			{
				intPsiLiquidEvaporation.value() += limitedAlphal_[iCell]*psiv[iCell]*mesh.V()[iCell];
			}
		}
		
		//- Calculate Nl and Nv
		dimensionedScalar Nl ("Nl", dimensionSet(0,0,0,0,0,0,0), 2.0);
		dimensionedScalar Nv ("Nv", dimensionSet(0,0,0,0,0,0,0), 2.0);
		
		reduce(intPsiLiquidEvaporation.value(),sumOp<scalar>());
		reduce(intPsiVaporEvaporation.value(),sumOp<scalar>());
		
		if (intPsiLiquidEvaporation.value() > 1e-99)
		{
		    Nl = intPsi0v/intPsiLiquidEvaporation;
		}
		if (intPsiVaporEvaporation.value() > 1e-99)
		{
		    Nv = intPsi0v/intPsiVaporEvaporation;
		}
		
		        
		//- Set source terms in cells with alpha1 < cutoff or alpha1 > 1-cutoff
		forAll(mesh.C(),iCell)
		{
			if (limitedAlphal_[iCell] < cutoff_)
			{
				mEvapAlphal_[iCell] = -Nv.value()*(1.0-limitedAlphal_[iCell])*psiv[iCell];
			}
			else if (limitedAlphal_[iCell] > 1.0-cutoff_)
			{
				mEvapAlphal_[iCell] = Nl.value()*limitedAlphal_[iCell]*psiv[iCell];
			}
			else
			{
				mEvapAlphal_[iCell] = 0.0;
			}
		}
	}
}

// * * * * * * * * * * * * * * Member Operators  * * * * * * * * * * * * * * //


// * * * * * * * * * * * * * * Friend Functions  * * * * * * * * * * * * * * //

// * * * * * * * * * * * * * * * Friend Operators  * * * * * * * * * * * * * //


// ************************************************************************* //