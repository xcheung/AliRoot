/***************************************************************************
 * Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
 *                                                                        *
 * Author: The ALICE Off-line Project.                                    *
 * Contributors are mentioned in the code where appropriate.              *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/

//-----------------------------------------------------//
//                                                     //
//           Date   : March 25 2004                    //
//  This reads the file PMD.RecPoints.root(TreeR),     //
//  calls the Clustering algorithm and stores the      //
//  clustering output in PMD.RecPoints.root(TreeR)     // 
//                                                     //
//-----------------------------------------------------//

#include <Riostream.h>
#include <TMath.h>
#include <TBRIK.h>
#include <TNode.h>
#include <TTree.h>
#include <TGeometry.h>
#include <TObjArray.h>
#include <TClonesArray.h>
#include <TFile.h>
#include <TBranch.h>
#include <TNtuple.h>
#include <TParticle.h>

#include "AliPMDcluster.h"
#include "AliPMDclupid.h"
#include "AliPMDrecpoint1.h"
#include "AliPMDUtility.h"
#include "AliPMDDiscriminator.h"
#include "AliPMDtracker.h"

#include "AliESDPmdTrack.h"
#include "AliESD.h"


ClassImp(AliPMDtracker)

AliPMDtracker::AliPMDtracker():
  fTreeR(0),
  fRecpoints(new TClonesArray("AliPMDrecpoint1", 1000)),
  fPMDcontin(new TObjArray()),
  fPMDcontout(new TObjArray()),
  fPMDdiscriminator(new AliPMDDiscriminator()),
  fPMDutil(new AliPMDUtility()),
  fPMDrecpoint(0),
  fPMDclin(0),
  fPMDclout(0),
  fDebug(0),
  fXvertex(0.),
  fYvertex(0.),
  fZvertex(0.),
  fSigmaX(0.),
  fSigmaY(0.),
  fSigmaZ(0.)
{
  //
  // Default Constructor
  //
}
//--------------------------------------------------------------------//
AliPMDtracker::~AliPMDtracker()
{
  // Destructor
  if (fRecpoints)
    {
      fRecpoints->Delete();
      delete fRecpoints;
      fRecpoints=0;
    }
  if (fPMDcontin)
    {
      fPMDcontin->Delete();
      delete fPMDcontin;
      fPMDcontin=0;
    }
  if (fPMDcontout)
    {
      fPMDcontout->Delete();
      delete fPMDcontout;
      fPMDcontout=0;
    }
}
//--------------------------------------------------------------------//
void AliPMDtracker::LoadClusters(TTree *treein)
{
  // Load the Reconstructed tree
  fTreeR = treein;
}
//--------------------------------------------------------------------//
void AliPMDtracker::Clusters2Tracks(AliESD *event)
{
  // Converts digits to recpoints after running clustering
  // algorithm on CPV plane and PREshower plane
  //

  Int_t   idet;
  Int_t   ismn;
  Float_t clusdata[5];

  TBranch *branch = fTreeR->GetBranch("PMDRecpoint");
  if (!branch) return;
  branch->SetAddress(&fRecpoints);  
  
  Int_t   nmodules = (Int_t) fTreeR->GetEntries();
  cout << " nmodules = " << nmodules << endl;
  for (Int_t imodule = 0; imodule < nmodules; imodule++)
    {
      fTreeR->GetEntry(imodule); 
      Int_t nentries = fRecpoints->GetLast();
      //      cout << " nentries = " << nentries << endl;
      for(Int_t ient = 0; ient < nentries+1; ient++)
	{
	  fPMDrecpoint = (AliPMDrecpoint1*)fRecpoints->UncheckedAt(ient);
	  idet        = fPMDrecpoint->GetDetector();
	  ismn        = fPMDrecpoint->GetSMNumber();
	  clusdata[0] = fPMDrecpoint->GetClusX();
	  clusdata[1] = fPMDrecpoint->GetClusY();
	  clusdata[2] = fPMDrecpoint->GetClusADC();
	  clusdata[3] = fPMDrecpoint->GetClusCells();
	  clusdata[4] = fPMDrecpoint->GetClusRadius();
	  
	  fPMDclin = new AliPMDcluster(idet,ismn,clusdata);
	  fPMDcontin->Add(fPMDclin);
	}
    }

  fPMDdiscriminator->Discrimination(fPMDcontin,fPMDcontout);

  const Float_t kzpos = 361.5;
  Int_t   ism =0, ium=0;
  Int_t   det,smn;
  Float_t xpos,ypos;
  Float_t xpad = 0, ypad = 0;
  Float_t adc, ncell, rad;
  Float_t xglobal, yglobal;
  Float_t pid;

  Float_t zglobal = kzpos + (Float_t) fZvertex;

  Int_t nentries2 = fPMDcontout->GetEntries();
  cout << " nentries2 = " << nentries2 << endl;
  for (Int_t ient1 = 0; ient1 < nentries2; ient1++)
    {
      fPMDclout = (AliPMDclupid*)fPMDcontout->UncheckedAt(ient1);
      
      det   = fPMDclout->GetDetector();
      smn   = fPMDclout->GetSMN();
      xpos  = fPMDclout->GetClusX();
      ypos  = fPMDclout->GetClusY();
      adc   = fPMDclout->GetClusADC();
      ncell = fPMDclout->GetClusCells();
      rad   = fPMDclout->GetClusRadius();
      pid   = fPMDclout->GetClusPID();
      
      //
      // Now change the xpos and ypos to its original values
      // for the unit modules which are earlier changed.
      // xpad and ypad are the real positions.
      //
      /**********************************************************************
       *    det   : Detector, 0: PRE & 1:CPV                                *
       *    smn   : Serial Module Number from which Super Module Number     *
       *            and Unit Module Numbers are extracted                   *
       *    xpos  : x-position of the cluster                               *
       *    ypos  : y-position of the cluster                               *
       *            THESE xpos & ypos are not the true xpos and ypos        *
       *            for some of the unit modules. They are rotated.         *
       *    adc   : ADC contained in the cluster                            *
       *    ncell : Number of cells contained in the cluster                *
       *    rad   : radius of the cluster (1d fit)                          *
       *    ism   : Supermodule number extracted from smn                   *
       *    ium   : Unit module number extracted from smn                   *
       *    xpad  : TRUE x-position of the cluster                          *
       *    ypad  : TRUE y-position of the cluster                          *
       **********************************************************************/
      //
      if(det == 0 || det == 1)
	{
	  if(smn < 12)
	    {
	      ism  = smn/6;
	      ium  = smn - ism*6;
	      xpad = ypos;
	      ypad = xpos;
	    }
	  else if( smn >= 12 && smn < 24)
	    {
	      ism  = smn/6;
	      ium  = smn - ism*6;
	      xpad = xpos;
	      ypad = ypos;
	    }
	}
     
      fPMDutil->RectGeomCellPos(ism,ium,xpad,ypad,xglobal,yglobal);
      fPMDutil->SetXYZ(xglobal,yglobal,zglobal);
      fPMDutil->CalculateEtaPhi();
      Float_t theta = fPMDutil->GetTheta();
      Float_t phi   = fPMDutil->GetPhi();

      // Fill ESD

      AliESDPmdTrack *esdpmdtr = new  AliESDPmdTrack();

      esdpmdtr->SetDetector(det);
      esdpmdtr->SetTheta(theta);
      esdpmdtr->SetPhi(phi);
      esdpmdtr->SetClusterADC(adc);
      esdpmdtr->SetClusterPID(pid);

      event->AddPmdTrack(esdpmdtr);
    }

}
//--------------------------------------------------------------------//
void AliPMDtracker::SetVertex(Double_t vtx[3], Double_t evtx[3])
{
  fXvertex = vtx[0];
  fYvertex = vtx[1];
  fZvertex = vtx[2];
  fSigmaX  = evtx[0];
  fSigmaY  = evtx[1];
  fSigmaZ  = evtx[2];
}
//--------------------------------------------------------------------//
void AliPMDtracker::SetDebug(Int_t idebug)
{
  fDebug = idebug;
}
//--------------------------------------------------------------------//
void AliPMDtracker::ResetClusters()
{
  if (fRecpoints) fRecpoints->Clear();
}
//--------------------------------------------------------------------//
