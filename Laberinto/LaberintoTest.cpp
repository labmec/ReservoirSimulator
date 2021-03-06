//$Id: main.cc,v 1.21 2010-07-22 17:43:43 caju Exp $
#ifdef HAVE_CONFIG_H
#include <pz_config.h>
#endif

#include "pzgeoel.h"
#include "pzgnode.h"
#include "pzgmesh.h"
#include "pzbiharmonic.h"
#include "pzcmesh.h"
#include "pzintel.h"
#include "pzcompel.h"

#include "pzfmatrix.h"
#include "pzvec.h"
#include "pzmanvector.h"
#include "pzstack.h"

#include "pzanalysis.h"
#include "pzfstrmatrix.h"
#include "pzskylstrmatrix.h"
#include "pzstepsolver.h"
#include "pzgeopyramid.h"
#include "TPZGeoLinear.h"

#include <TPZRefPattern.h>

#include "TPZMaterial.h"
#include "pzelasmat.h"
#include "pzlog.h"

#include "pzgengrid.h"

#include <time.h>
#include <stdio.h>

#include <math.h>

#include <iostream>
#include <fstream>
#include <string.h>
#include <sstream>

#include <set>
#include <map>
#include <vector>
#include "TPZRefPatternDataBase.h"
#include "TPZRefPatternTools.h"
#include "TPZVTKGeoMesh.h"
#include "TPZExtendGridDimension.h"

#include <opencv2/opencv.hpp>

#include "TRSLinearInterpolator.h"
#include "TPZMatLaplacian.h"
#include "pzpoisson3d.h"
#include "mixedpoisson.h"
#include "pzbndcond.h"
#include "TPZSSpStructMatrix.h"
#include "pzskylstrmatrix.h"
#include "TPZSkylineNSymStructMatrix.h"
#include "TPZHybridizeHDiv.h"
#include "pzbuildmultiphysicsmesh.h"
#include "TPZMultiphasicFlowAnalysis.h"
//#include "TPZAxiSymmetricDarcyFlow.h"

using namespace std;
using namespace cv;

// Creating the computational H1 mesh
TPZCompMesh *CMeshH1(TPZGeoMesh *gmesh, int p_order);
TPZCompMesh *CMeshFlux(TPZGeoMesh * gmesh,int pOrder);
TPZCompMesh *CMeshPressure(TPZGeoMesh * gmesh, int pOrder);
TPZCompMesh *CMeshMultphysics(TPZGeoMesh * gmesh, TPZVec<TPZCompMesh *> meshvec);
TPZGeoMesh *GeoMeshFromPng(string name);

int H1Test();
int MixedTest();

int main(){
    int MixedTest();
//    TPZMultiphasicFlowAnalysis test;
//
//    TPZManVector<REAL,3> x0(3,0.),x1(3,200);
//    x1[2] = 0.;
//    TPZManVector<int,2> nelx(2,200);
//    nelx[0] = 200;
//    TPZGenGrid gengrid(nelx,x0,x1);
//    gengrid.SetElementType(EQuadrilateral);
//    TPZGeoMesh *gmesh = new TPZGeoMesh;
//    gmesh->SetDimension(2);
//    gengrid.Read(gmesh);
//    std::ofstream sim81("sim81test.txt");
//    std::ofstream sim82("sim81test.vtk");
//    gmesh->BuildConnectivity();
//    TPZVTKGeoMesh::PrintGMeshVTK(gmesh, sim82);
//    gmesh->Print(sim81);
//    TPZCompMesh *meshsim81 = new TPZCompMesh(gmesh);
//
    
    
}

int MixedTest(){
    
    TPZGeoMesh *gmesh = GeoMeshFromPng("MAZE128x128.png");
    int flux_order = 1;
    int p_order = 1;
    
    {
#ifdef PZDEBUG
        std::ofstream file("maze.txt");
        gmesh->Print(file);
        
        std::ofstream out("maze.vtk");
        TPZVTKGeoMesh::PrintGMeshVTK(gmesh, out, true);
#endif
    }
    
    TPZCompMesh *cmesh_flux =CMeshFlux(gmesh,flux_order);
    TPZCompMesh *cmesh_presure =CMeshPressure(gmesh,p_order);
    
  
    TPZVec<TPZCompMesh *> fmeshvec(2);
    fmeshvec=cmesh_flux;
    fmeshvec[1]=cmesh_presure;
    gmesh->ResetReference();
    
    TPZCompMesh *MixedMesh = CMeshMultphysics(gmesh,fmeshvec);
    
    std::ofstream file("MixedCMesh.vtk");
    TPZVTKGeoMesh::PrintCMeshVTK(MixedMesh, file);
    
    std::ofstream out("MixedCMesh.txt");
//    MixedMesh->Print(out);
    

    
    std::cout << "number of equations = " << MixedMesh->NEquations() << std::endl;
    

    
    //Solving the system:
    bool optimizeBandwidth = true;
    MixedMesh->InitializeBlock();
    
    TPZCompMesh * cmesh_m_Hybrid;
    TPZManVector<TPZCompMesh*, 3> meshvector_Hybrid(3);
    TPZHybridizeHDiv hybridizer;
    tie(cmesh_m_Hybrid, meshvector_Hybrid) = hybridizer.Hybridize(MixedMesh, fmeshvec, true, -1.);
    cmesh_m_Hybrid->InitializeBlock();
    
    bool must_opt_band_width_Q = true;
    int number_threads = 4;
    TPZAnalysis *an = new TPZAnalysis(cmesh_m_Hybrid,must_opt_band_width_Q);
    
    //
    TPZSymetricSpStructMatrix sparse_matrix(cmesh_m_Hybrid);
    TPZStepSolver<STATE> step;
    sparse_matrix.SetNumThreads(number_threads);
    step.SetDirect(ELDLt);
    an->SetStructuralMatrix(sparse_matrix);
    an->SetSolver(step);
    
   // TPZBuildMultiphysicsMesh::TransferFromMultiPhysics(fmeshvec, MixedMesh);
    
    
    
    // Solving the LS
    an->Assemble();
    
//    an->Solver().Matrix()->Print("j = ", std::cout,EMathematicaInput);
//    an->Rhs().Print("r = ", std::cout,EMathematicaInput);
    
    an->Solve();

    
    //POS
    TPZManVector<std::string,10> scalnames(2), vecnames(1);
    vecnames[0]  = "Flux";
    
    scalnames[0] = "Pressure";
    scalnames[1] = "Permeability";



    const int dim = an->Mesh()->Dimension();
    int div = 0;

    an->DefineGraphMesh(dim,scalnames,vecnames,"hdiv.vtk");
    an->PostProcess(div,dim);
    std::cout << "Standard post-processing finished." << std::endl;
    
    return 0;
}

int H1Test()
{
   
    TPZGeoMesh *gmesh = GeoMeshFromPng("MAZE128x128.png");
    {
#ifdef PZDEBUG
        std::ofstream file("mazeh1.txt");
        gmesh->Print(file);
        
        std::ofstream out("mazeh1.vtk");
        TPZVTKGeoMesh::PrintGMeshVTK(gmesh, out, true);
        
#endif
    }

    //Creando a malla computacional
    int p_order = 1 ;
    int number_threads = 4;
    bool must_opt_band_width_Q = true;
    TPZCompMesh *cmesh = CMeshH1(gmesh,p_order);
    TPZAnalysis *an = new TPZAnalysis(cmesh,must_opt_band_width_Q);
   
#ifdef USING_MKL
    TPZSymetricSpStructMatrix struct_mat(cmesh);
    struct_mat.SetNumThreads(number_threads);
    an->SetStructuralMatrix(struct_mat);
#else
    TPZSkylineStructMatrix struct_mat(cmesh);
    struct_mat.SetNumThreads(number_threads);
    an->SetStructuralMatrix(struct_mat);
#endif
    TPZStepSolver<STATE> step;
    step.SetDirect(ECholesky);
    an->SetSolver(step);
    
    
    // Solving the LS
    an->Assemble();
    an->Solve();
    
    // post-processing step
    {
        const int dim = an->Mesh()->Dimension();
        int div = 0;
        std::string plotfile = "h1_approximation.vtk";
        TPZStack<std::string> scalar_names, vec_names;
        
        scalar_names.push_back("Solution");
        vec_names.push_back("MinusKGradU");
        an->DefineGraphMesh(dim,scalar_names,vec_names,plotfile);
        an->PostProcess(div,dim);
        std::cout << "Standard post-processing finished." << std::endl;
    }
    
    return 0;
}
TPZCompMesh *CMeshH1(TPZGeoMesh *gmesh, int p_order){
    
    int impervious_mat = 1;
    int permeable_mat = 2;
    int dim = gmesh->Dimension();
    
    REAL perm_0 = 1.0e-3;
    REAL perm_1 = 1000.0;
    
    REAL conv=0;
    TPZVec<REAL> convdir(dim,0.0);
    
    TPZCompMesh *cmesh = new TPZCompMesh(gmesh);
    cmesh->SetDimModel(dim);
    
    TPZMatPoisson3d *mat_0 = new TPZMatPoisson3d(impervious_mat,dim);
    TPZMatPoisson3d *mat_1 = new TPZMatPoisson3d(permeable_mat,dim);
    mat_0->SetParameters(perm_0, conv, convdir);
    mat_1->SetParameters(perm_1, conv, convdir);
    
    //  inserting volumetric materials objects
    cmesh->InsertMaterialObject(mat_0);
    cmesh->InsertMaterialObject(mat_1);
    
    
    int type_D = 0;
    int type_N = 1;
    TPZFMatrix<STATE> val1(1, 1, 0.), val2(1, 1, 0.);
    
    // Insert boundary conditions
    //Neumann boundary conditions (flux = 0)
    int right_bc_id = -2;
    val2(0,0) = 0.0;
    TPZMaterial * right_bc = mat_0->CreateBC(mat_0, right_bc_id, type_N, val1, val2);
    cmesh->InsertMaterialObject(right_bc);
    
    int left_bc_id = -4;
    val2(0,0) = 0.0;
    TPZMaterial * left_bc = mat_0->CreateBC(mat_0, left_bc_id, type_N, val1, val2);
    cmesh->InsertMaterialObject(left_bc);
    
    int bottom_bc_1id = -1;
    val2(0,0) = 0.0;
    TPZMaterial * bottom_bc_1 = mat_0->CreateBC(mat_0, bottom_bc_1id, type_N, val1, val2);
    cmesh->InsertMaterialObject(bottom_bc_1);
    
    int top_bc_1id = -3;
    val2(0,0) = 0.0;
    TPZMaterial * top_bc_1 = mat_0->CreateBC(mat_0, top_bc_1id, type_N, val1, val2);
    cmesh->InsertMaterialObject(top_bc_1);
    

    //Dirichlet Conditions (p=1 in, p=0 out)
    int bottom_bc_id = -5;
    val2(0,0) = 1000.0;
    TPZMaterial * bottom_bc = mat_0->CreateBC(mat_0, bottom_bc_id, type_D, val1, val2);
    cmesh->InsertMaterialObject(bottom_bc);
    
    int top_bc_id = -6;
    val2(0,0) = 0.0;
    TPZMaterial * top_bc = mat_0->CreateBC(mat_0, top_bc_id, type_D, val1, val2);
    cmesh->InsertMaterialObject(top_bc);

    cmesh->SetName("LaberintoTest");
    cmesh->SetAllCreateFunctionsContinuous();
    cmesh->SetDefaultOrder(p_order);
    cmesh->AutoBuild();

    

#ifdef PZDEBUG
    std::ofstream file("cmesh_h.txt");
    cmesh->Print(file);
#endif
    
    return cmesh;
}

TPZCompMesh *CMeshFlux(TPZGeoMesh * gmesh,int pOrder){
    
    int impervious_mat = 1;
    int permeable_mat = 2;
    int dim = gmesh->Dimension();
    
    REAL perm_0 = 1.0e-3;
    REAL perm_1 = 10;
    
    REAL conv=0;
    TPZVec<REAL> convdir(dim,0.0);
    
    TPZCompMesh *cmesh = new TPZCompMesh(gmesh);
    cmesh->SetDimModel(dim);
    cmesh->SetDefaultOrder(pOrder); //Default polynomial order of the approximation
    
    //Definition of the approximation space:
    
    TPZMatPoisson3d *mat_0 = new TPZMatPoisson3d(impervious_mat,dim);
    TPZMatPoisson3d *mat_1 = new TPZMatPoisson3d(permeable_mat,dim);
    mat_0->SetParameters(perm_0, conv, convdir);
    mat_1->SetParameters(perm_1, conv, convdir);
    
    //  inserting volumetric materials objects
    cmesh->InsertMaterialObject(mat_0);
    cmesh->InsertMaterialObject(mat_1);
    
    cmesh->SetAllCreateFunctionsHDiv(); //Creating H(div) functions

    
    int type_D = 0;
    int type_N = 1;
    TPZFMatrix<STATE> val1(1, 1, 0.), val2(1, 1, 0.);
    
    // Insert boundary conditions
    //Neumann boundary conditions (flux = 0)
    int right_bc_id = -2;
    TPZMaterial * right_bc = mat_0->CreateBC(mat_0, right_bc_id, type_N, val1, val2);
    cmesh->InsertMaterialObject(right_bc);
    
    int left_bc_id = -4;
    TPZMaterial * left_bc = mat_0->CreateBC(mat_0, left_bc_id, type_N, val1, val2);
    cmesh->InsertMaterialObject(left_bc);
    
    int bottom_bc_1id = -1;
    TPZMaterial * bottom_bc_1 = mat_0->CreateBC(mat_0, bottom_bc_1id, type_N, val1, val2);
    cmesh->InsertMaterialObject(bottom_bc_1);
    
    int top_bc_1id = -3;
    TPZMaterial * top_bc_1 = mat_0->CreateBC(mat_0, top_bc_1id, type_N, val1, val2);
    cmesh->InsertMaterialObject(top_bc_1);
    
    
    //Dirichlet Conditions (p=1 in, p=0 out)
    int bottom_bc_id = -5;
    TPZMaterial * bottom_bc = mat_0->CreateBC(mat_0, bottom_bc_id, type_D, val1, val2);
    cmesh->InsertMaterialObject(bottom_bc);
    
    int top_bc_id = -6;
    TPZMaterial * top_bc = mat_0->CreateBC(mat_0, top_bc_id, type_D, val1, val2);
    cmesh->InsertMaterialObject(top_bc);
    
    cmesh->SetName("LaberintoTest");
    cmesh->AutoBuild();
    
#ifdef PZDEBUG
    std::ofstream file("cmesh_flux.txt");
    cmesh->Print(file);
#endif
    
    return cmesh;
    
}
TPZCompMesh *CMeshPressure(TPZGeoMesh * gmesh, int pOrder){
    
    int impervious_mat = 1;
    int permeable_mat = 2;
    int dim = gmesh->Dimension();
    
    REAL perm_0 = 1.0e-3;
    REAL perm_1 = 10;
    
    REAL conv=0;
    TPZVec<REAL> convdir(dim,0.0);
    
    TPZCompMesh *cmesh = new TPZCompMesh(gmesh);
    cmesh->SetDimModel(dim);

    cmesh->SetDefaultOrder(pOrder); //Default polynomial order of the approximation
    cmesh->SetAllCreateFunctionsDiscontinuous();
    cmesh->ApproxSpace().CreateDisconnectedElements(true);

    
    TPZMatPoisson3d *mat_0 = new TPZMatPoisson3d(impervious_mat,dim);
    TPZMatPoisson3d *mat_1 = new TPZMatPoisson3d(permeable_mat,dim);
    mat_0->SetParameters(perm_0, conv, convdir);
    mat_1->SetParameters(perm_1, conv, convdir);
    
    //  inserting volumetric materials objects
    cmesh->InsertMaterialObject(mat_0);
    cmesh->InsertMaterialObject(mat_1);
    
    
//    int type_D = 0;
//    int type_N = 1;
//    TPZFMatrix<STATE> val1(1, 1, 0.), val2(1, 1, 0.);
//
//    // Insert boundary conditions
//    //Neumann boundary conditions (flux = 0)
//    int right_bc_id = -2;
//    val2(0,0) = 0.0;
//    TPZMaterial * right_bc = mat_0->CreateBC(mat_0, right_bc_id, type_N, val1, val2);
//    cmesh->InsertMaterialObject(right_bc);
//
//    int left_bc_id = -4;
//    val2(0,0) = 0.0;
//    TPZMaterial * left_bc = mat_0->CreateBC(mat_0, left_bc_id, type_N, val1, val2);
//    cmesh->InsertMaterialObject(left_bc);
//
//    int bottom_bc_1id = -1;
//    val2(0,0) = 0.0;
//    TPZMaterial * bottom_bc_1 = mat_0->CreateBC(mat_0, bottom_bc_1id, type_N, val1, val2);
//    cmesh->InsertMaterialObject(bottom_bc_1);
//
//    int top_bc_1id = -3;
//    val2(0,0) = 0.0;
//    TPZMaterial * top_bc_1 = mat_0->CreateBC(mat_0, top_bc_1id, type_N, val1, val2);
//    cmesh->InsertMaterialObject(top_bc_1);
//
//
//    //Dirichlet Conditions (p=1 in, p=0 out)
//    int bottom_bc_id = -5;
//    val2(0,0) = 100.0;
//    TPZMaterial * bottom_bc = mat_0->CreateBC(mat_0, bottom_bc_id, type_D, val1, val2);
//    cmesh->InsertMaterialObject(bottom_bc);
//
//    int top_bc_id = -6;
//    val2(0,0) = 10.0;
//    TPZMaterial * top_bc = mat_0->CreateBC(mat_0, top_bc_id, type_D, val1, val2);
//    cmesh->InsertMaterialObject(top_bc);
    
    cmesh->SetName("Pressure");
    cmesh->AutoBuild();
    
    int ncon = cmesh->NConnects();
    for(int i=0; i<ncon; i++)
    {
        TPZConnect &newnod = cmesh->ConnectVec()[i];
        newnod.SetLagrangeMultiplier(1);
    }
    
#ifdef PZDEBUG
    std::ofstream out("cmeshPress.txt");
    cmesh->Print(out);
#endif
    
    return cmesh;

    
}
TPZGeoMesh *GeoMeshFromPng(string name){
    const int bcDL = -1;
    const int bcB = -2;
    const int bcDR = -3;
    const int bcDT = -4;
    
    
    //  Mat image = imread("normal.png",IMREAD_GRAYSCALE);
    Mat image = imread(name,IMREAD_GRAYSCALE);
    //    Mat image = imread("single_quad.png",IMREAD_GRAYSCALE);
    int k=0;
    int px=image.size[0];
    int py=image.size[1];
    int p =px*py;
    if(p==0){
        DebugStop();
    }
    vector<int> vec(p,0);
    
    for (int i = 0; i<px; ++i) {
        for (int j = py  ; j>0; --j) {
            int val =(int)image.at<uchar>(Point(j, i));
            if (val>200){
                val=255;
            }
           int pix =val/255;
            vec[p-k]=pix;
            
            
            
            k++;
        }
    }
    
    
    
    // Creating the Geo mesh
    TPZManVector<REAL,3> x0(3,0.),x1(3,px);
    x1[2] = 0.;
    TPZManVector<int,2> nelx(2,py);
    nelx[0] = px;
    TPZGenGrid gengrid(nelx,x0,x1);
    gengrid.SetElementType(EQuadrilateral);
    TPZGeoMesh *gmesh = new TPZGeoMesh;
    gmesh->SetDimension(2);
    gengrid.Read(gmesh);
    //gengrid.Read(gmesh,2);
    
    //MatsID
    int nels = gmesh->NElements();
    TPZGeoEl *gel_in;
    TPZGeoEl *gel_out;
    TPZGeoEl *gel_in1D;
    TPZGeoEl *gel_out1D;
  
    
    for (int i=0; i<nels; i++) {
        TPZGeoEl *gel =gmesh->Element(i);
        gel->SetMaterialId(vec[i]+ 1 );
       
        if (i<= px) {
            if((vec[i]+1)==2){
                gel_in =gel;
            }
        }
        
        if (i >= (px)*(py-1)) {
            if((vec[i]+1)==2){
                gel_out=gel;
            }
        }
        
    }
    
    //gengrid.SetBC(TPZGeoMesh *gr, int side, int bc)
    gengrid.SetBC(gmesh, 4, bcDL);
    gengrid.SetBC(gmesh, 5, bcB);
    gengrid.SetBC(gmesh, 6, bcDR);
    gengrid.SetBC(gmesh, 7, bcDT);
    
  
    int gel_in_index = gel_in->Index();
    gel_in1D = gmesh->Element(gel_in_index)->Neighbour(4).Element();
    gel_in1D->SetMaterialId(-5);

    int gel_out_index = gel_out->Index();
    gel_out1D = gmesh->Element(gel_out_index)->Neighbour(6).Element();
    gel_out1D->SetMaterialId(-6);

    
    gmesh->BuildConnectivity();
    return gmesh;
}
TPZCompMesh *CMeshMultphysics(TPZGeoMesh * gmesh, TPZVec<TPZCompMesh *> meshvec){
   
    //Creating computational mesh for multiphysic elements
    TPZCompMesh *mphysics = new TPZCompMesh(gmesh);
    
    int impervious_mat = 1;
    int permeable_mat = 2;
    int dim = gmesh->Dimension();
    
    REAL perm_0 = 1.0e-3;
    REAL perm_1 = 1000.0;
    
    REAL conv=0;
    TPZVec<REAL> convdir(dim,0.0);
    
    std::cout<<mphysics->NMaterials();
   
    TPZMixedPoisson *mat_0 = new TPZMixedPoisson(impervious_mat,dim);
    mat_0->SetPermeability(perm_0);
    
    
    TPZMixedPoisson *mat_1 = new TPZMixedPoisson(permeable_mat,dim);
    mat_1->SetPermeability(perm_1);
 
    mat_0->SetParameters(perm_0, conv, convdir);
    mat_1->SetParameters(perm_1, conv, convdir);
    
    mphysics->InsertMaterialObject(mat_0);
    mphysics->InsertMaterialObject(mat_1);
 
        //Inserir condicoes de contorno
    
    
    int type_D = 0;
    int type_N = 1;
    TPZFMatrix<STATE> val1(1, 1, 0.), val2(1, 1, 0.);
    
    // Insert boundary conditions
    //Neumann boundary conditions (flux = 0)
    int right_bc_id = -2;
    val2(0,0) = 0.0;
    TPZMaterial * right_bc = mat_0->CreateBC(mat_0, right_bc_id, type_N, val1, val2);
    mphysics->InsertMaterialObject(right_bc);
    
    
    int left_bc_id = -4;
    val2(0,0) = 0.0;
    TPZMaterial * left_bc = mat_0->CreateBC(mat_0, left_bc_id, type_N, val1, val2);
    mphysics->InsertMaterialObject(left_bc);
    
    int bottom_bc_1id = -1;
    val2(0,0) = 0;
    TPZMaterial * bottom_bc_1 = mat_0->CreateBC(mat_0, bottom_bc_1id, type_N, val1, val2);
    mphysics->InsertMaterialObject(bottom_bc_1);
    
    int top_bc_1id = -3;
    val2(0,0) = 0.0;
    TPZMaterial * top_bc_1 = mat_0->CreateBC(mat_0, top_bc_1id, type_N, val1, val2);
    mphysics->InsertMaterialObject(top_bc_1);
    
    //Dirichlet Conditions (p=1 in, p=0 out)
    int bottom_bc_id = -5;
    val2(0,0) = 1000.0;
    TPZMaterial * bottom_bc = mat_0->CreateBC(mat_0, bottom_bc_id, type_D, val1, val2);
    mphysics->InsertMaterialObject(bottom_bc);
    
    int top_bc_id = -6;
    val2(0,0) = 0.0;
    TPZMaterial * top_bc = mat_0->CreateBC(mat_0, top_bc_id, type_D, val1, val2);
    mphysics->InsertMaterialObject(top_bc);
    
    mphysics->SetAllCreateFunctionsMultiphysicElem();
    mphysics->SetDimModel(gmesh->Dimension());
    mphysics->AutoBuild();
    
    TPZBuildMultiphysicsMesh::AddElements(meshvec, mphysics);
    TPZBuildMultiphysicsMesh::AddConnects(meshvec,mphysics);
    TPZBuildMultiphysicsMesh::TransferFromMeshes(meshvec, mphysics);
    
#ifdef PZDEBUG
    std::ofstream file("cmesh_mphysics.txt");
    mphysics->Print(file);
#endif
    
    return mphysics;
}
