/**************************************************************************
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

//-----------------------------------------------------------------------
// Example of task (running locally, on AliEn and CAF),
// which provides standard way of calculating acceptance and efficiency
// between different steps of the procedure.
// The ouptut of the task is a AliCFContainer from which the efficiencies
// can be calculated
//-----------------------------------------------------------------------
// Author : Marta Verweij - UU
//-----------------------------------------------------------------------


#ifndef ALIPWG4HighPtSpectra_CXX
#define ALIPWG4HighPtSpectra_CXX

#include "AliPWG4HighPtSpectra.h"

#include "AliStack.h"
#include "TParticle.h"
#include "TH1I.h"
#include "AliMCEvent.h"
#include "AliMCEventHandler.h"
#include "AliAnalysisManager.h"
#include "AliCFContainer.h"
#include "TChain.h"
#include "AliESDtrack.h"
#include "AliESDtrackCuts.h"
#include "AliExternalTrackParam.h"
#include "AliESDInputHandler.h"

using namespace std; //required for resolving the 'cout' symbol

ClassImp(AliPWG4HighPtSpectra)

//__________________________________________________________________________
AliPWG4HighPtSpectra::AliPWG4HighPtSpectra() : AliAnalysisTask("AliPWG4HighPtSpectra", ""), 
  fReadAODData(0),
  fCFManager(0x0),
  fESD(0),
  fTrackCuts(0),
  fHistList(0),
  fHistEventsProcessed(0x0)
{
  //
  //Default ctor
  //
}
//___________________________________________________________________________
AliPWG4HighPtSpectra::AliPWG4HighPtSpectra(const Char_t* name) :
  AliAnalysisTask(name,""),
  fReadAODData(0),
  fCFManager(0x0),
  fESD(0),
  fTrackCuts(new AliESDtrackCuts),
  fHistList(0),
  fHistEventsProcessed(0x0)
{
  //
  // Constructor. Initialization of Inputs and Outputs
  //
  Info("AliPWG4HighPtSpectra","Calling Constructor");
  // Input slot #0 works with a TChain ESD
  DefineInput(0, TChain::Class());
  DefineOutput(0,TList::Class());
  DefineOutput(1,AliCFContainer::Class());
}

//___________________________________________________________________________
AliPWG4HighPtSpectra& AliPWG4HighPtSpectra::operator=(const AliPWG4HighPtSpectra& c) 
{
  //
  // Assignment operator
  //
  if (this!=&c) {
    AliAnalysisTask::operator=(c) ;
    fReadAODData = c.fReadAODData ;
    fCFManager  = c.fCFManager;
    fHistList = c.fHistList;
    fHistEventsProcessed = c.fHistEventsProcessed;
  }
  return *this;
}

//___________________________________________________________________________
AliPWG4HighPtSpectra::AliPWG4HighPtSpectra(const AliPWG4HighPtSpectra& c) :
  AliAnalysisTask(c),
  fReadAODData(c.fReadAODData),
  fCFManager(c.fCFManager),
  fESD(c.fESD),
  fTrackCuts(c.fTrackCuts),
  fHistList(c.fHistList),
  fHistEventsProcessed(c.fHistEventsProcessed)
{
  //
  // Copy Constructor
  //
}

//___________________________________________________________________________
AliPWG4HighPtSpectra::~AliPWG4HighPtSpectra() {
  //
  //destructor
  //
  Info("~AliPWG4HighPtSpectra","Calling Destructor");
  if (fCFManager)           delete fCFManager ;
  if (fHistEventsProcessed) delete fHistEventsProcessed ;
}
//________________________________________________________________________
void AliPWG4HighPtSpectra::ConnectInputData(Option_t *) 
{
  // Connect ESD here
  // Called once
  printf(">> AliPWG4HighPtSpectra::ConnectInputData \n");

  TTree* tree = dynamic_cast<TTree*> (GetInputData(0));
  if (!tree) {
    Printf("ERROR: Could not read chain from input slot 0");
  } else {
    
    AliESDInputHandler *esdH = dynamic_cast<AliESDInputHandler*> (AliAnalysisManager::GetAnalysisManager()->GetInputEventHandler());
    
    if (!esdH) {
      Printf("ERROR: Could not get ESDInputHandler");
    } else
      fESD = esdH->GetEvent();
  }
 
}
//_________________________________________________
void AliPWG4HighPtSpectra::Exec(Option_t *)//UserExec(Option_t *)
{
  //
  // Main loop function
  //
  Info("Exec","") ;

  if (!fESD) {
    Printf("ERROR: fESD not available");
    return;
  }

  // Process MC truth, therefore we receive the AliAnalysisManager and ask it for the AliMCEventHandler
  // This handler can return the current MC event

  AliMCEventHandler* eventHandler = dynamic_cast<AliMCEventHandler*> (AliAnalysisManager::GetAnalysisManager()->GetMCtruthEventHandler());
  if (!eventHandler) {
    Printf("ERROR: Could not retrieve MC event handler");
    return;
  }

  AliMCEvent* mcEvent = eventHandler->MCEvent();
  if (!mcEvent) {
    Printf("ERROR: Could not retrieve MC event");
    return;
  }

  Printf("MC particles: %d", mcEvent->GetNumberOfTracks());

  if (!fESD) {
    Printf("ERROR: fESD not available");
    return;
  }

  AliStack* stack = mcEvent->Stack();                //Particles Stack

  Printf("MC particles stack: %d", stack->GetNtrack());

  const AliESDVertex *vtx = fESD->GetPrimaryVertex();

  // Need vertex cut
  if (vtx->GetNContributors() < 2) return;
  double pvtx[3];
  vtx->GetXYZ(pvtx);
  if(TMath::Abs(pvtx[2])>10.) return;

  printf("Vertex title %s, status %d, nCont %d\n",vtx->GetTitle(), vtx->GetStatus(), vtx->GetNContributors());
  // Need to keep track of evts without vertex

  Int_t nTracks = fESD->GetNumberOfTracks();
  AliDebug(2,Form("nTracks %d", nTracks));
  printf("nTracks %d\n", nTracks);

  // AliVEvent*    fEvent = fInputEvent ;
  
//   if (!fEvent) {
//     Error("UserExec","NO EVENT FOUND!");
//     return;
//   }

//   //pass the MC evt handler to the cuts that need it 
//   fCFManager->SetEventInfo(fMCEvent);

  Double_t containerInputRec[1] ;
  Double_t containerInputTPConly[1] ;
  Double_t containerInputMC[1] ;
  //Now go to rec level
  for (Int_t iTrack = 0; iTrack<nTracks; iTrack++) 
    {   
      if(!fESD->GetTrack(iTrack) ) continue;
      AliESDtrack* track = fESD->GetTrack(iTrack);
      if(!(AliExternalTrackParam *)track->GetTPCInnerParam()) continue;
      AliExternalTrackParam *trackTPC = (AliExternalTrackParam *)track->GetTPCInnerParam();
      if(!track || !trackTPC) continue;

      Int_t label = TMath::Abs(track->GetLabel());
      TParticle *particle = stack->Particle(label) ;
      if(!particle) continue;

      
      //fill the container
      containerInputRec[0] = track->Pt();
      containerInputTPConly[0] = trackTPC->Pt();
      containerInputMC[0] = particle->Pt();
      
      if (fTrackCuts->AcceptTrack(track)) {
	fCFManager->GetParticleContainer()->Fill(containerInputRec,kStepReconstructed);
	fCFManager->GetParticleContainer()->Fill(containerInputTPConly,kStepReconstructedTPCOnly);
	fCFManager->GetParticleContainer()->Fill(containerInputMC,kStepReconstructedMC);
      }
      //      if (!fCFManager->CheckParticleCuts(1,track)) continue ;
      if ( fTrackCuts->AcceptTrack(track) && !stack->IsPhysicalPrimary(label) ) {
	fCFManager->GetParticleContainer()->Fill(containerInputRec,kStepSecondaries);
	// 	int label_mom = TMath::Abs(particle->GetFirstMother());
	// 	TParticle *mother = stack->Particle(label_mom);
	// 	if(!mother) continue;
	// 	cout << "#Daughters mom: " << mother->GetNDaughters() << "\tPDGmom: " << mother->GetPdgCode() << "\tPDGdaughter: " << particle->GetPdgCode() << endl;
      }
      
    }

  for(int iPart = 1; iPart<(mcEvent->GetNumberOfTracks()); iPart++)//stack->GetNprimary();
     {
       //       TParticle *part = stack->Particle(iPart);
       AliMCParticle *mcPart  = (AliMCParticle*)mcEvent->GetTrack(iPart);
       if(!mcPart) continue;

       //fill the container
       containerInputMC[0] = mcPart->Pt();
 
       if (!fCFManager->CheckParticleCuts(3,mcPart)) continue ;
 
       int counter;
 
       Float_t trackLengthTPC = mcPart->GetTPCTrackLength(fESD->GetMagneticField(),0.1,counter,3.0);
       //       printf("TPCTrackLength %f \t %f  \n", TPCTrackLength,fESD->GetMagneticField());

       if(trackLengthTPC>80.) fCFManager->GetParticleContainer()->Fill(containerInputMC,kStepMCtrackable) ;

     }

   fHistEventsProcessed->Fill(0);
   PostData(0,fHistList);
   PostData(1,fCFManager->GetParticleContainer());

}


//___________________________________________________________________________
void AliPWG4HighPtSpectra::Terminate(Option_t*)
{
  // The Terminate() function is the last function to be called during
  // a query. It always runs on the client, it can be used to present
  // the results graphically or save the results to file.

  Info("Terminate","");
//   AliAnalysisTaskSE::Terminate();

}


//___________________________________________________________________________
void AliPWG4HighPtSpectra::CreateOutputObjects() {
  //HERE ONE CAN CREATE OUTPUT OBJECTS, IN PARTICULAR IF THE OBJECT PARAMETERS DON'T NEED
  //TO BE SET BEFORE THE EXECUTION OF THE TASK
  //
  Info("CreateOutputObjects","CreateOutputObjects of task %s", GetName());

  OpenFile(0);
  fHistList = new TList();
  //slot #1
  //  OpenFile(0);
  fHistEventsProcessed = new TH1I("fHistEventsProcessed","",1,0,1) ;
  fHistList->Add(fHistEventsProcessed);

}

#endif
