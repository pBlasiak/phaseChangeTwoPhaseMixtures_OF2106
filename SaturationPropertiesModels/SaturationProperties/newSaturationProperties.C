/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2015 OpenFOAM Foundation
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

#include "SaturationProperties.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

Foam::autoPtr<Foam::SaturationProperties>
Foam::SaturationProperties::New
(
    const volVectorField& U,
    const surfaceScalarField& phi
)
{
    IOdictionary phaseChangePropertiesDict
    (
        IOobject
        (
            "phaseChangeProperties",
            U.time().constant(),
            U.db(),
            IOobject::MUST_READ,
            IOobject::NO_WRITE,
            false
        )
    );

    word SaturationPropertiesTypeName
    (
        phaseChangePropertiesDict.lookup("saturationPropertiesModel")
    );

    Info<< "Selecting "
        << SaturationPropertiesTypeName << endl;

    auto* ctorPtr = componentsConstructorTable(SaturationPropertiesTypeName);

    //componentsConstructorTable::iterator cstrIter =
    //    componentsConstructorTablePtr_
    //        ->find(SaturationPropertiesTypeName);

    //if (cstrIter == componentsConstructorTablePtr_->end())
    //{
    //    FatalErrorInFunction
    //        << "Unknown SaturationProperties type "
    //        << SaturationPropertiesTypeName << endl << endl
    //        << "Valid  SaturationPropertiess are : " << endl
    //        << componentsConstructorTablePtr_->sortedToc()
    //        << exit(FatalError);
    //}

    //return autoPtr<SaturationProperties>(cstrIter()(U, phi));
    if (!ctorPtr)
    {
        FatalIOErrorInLookup
        (
            phaseChangePropertiesDict,
            "SaturationProperties",
            SaturationPropertiesTypeName,
            *componentsConstructorTablePtr_
        ) << exit(FatalIOError);
    }

    return autoPtr<SaturationProperties>(ctorPtr(U, phi));
}


// ************************************************************************* //
