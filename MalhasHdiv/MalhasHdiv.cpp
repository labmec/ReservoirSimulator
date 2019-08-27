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
#include "TPZHybridizeHDiv.h"
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

#include "TRSLinearInterpolator.h"
#include "TPZMatLaplacian.h"
#include "pzpoisson3d.h"
#include "TPZNullMaterial.h"
#include "mixedpoisson.h"
#include "pzbndcond.h"
#include "TPZSSpStructMatrix.h"
#include "pzskylstrmatrix.h"
#include "TPZSkylineNSymStructMatrix.h"

#include "pzanalysis.h"
#include "pzfstrmatrix.h"
#include "pzskylstrmatrix.h"
#include "pzstepsolver.h"
#include "pzsolve.h"
#include "TPZPersistenceManager.h"
#include "pzbuildmultiphysicsmesh.h"
#include "TPZMultiphysicsCompMesh.h"
#include "TPZCompMeshTools.h"
#include "TPZMixedDarcyWithFourSpaces.h"
#include "pzcmesh.h"
#include "pzbdstrmatrix.h"

#include "TPZHdivTransfer.h"
#include "pzseqsolver.h"
#include "pzmgsolver.h"

#ifdef _AUTODIFF
#include "tfad.h"
#include "fad.h"
#include "pzextractval.h"
#endif


//Creating geometric 1D and 2D mesh
TPZGeoMesh * GenerateGmeshOne(int nx, double l);
TPZGeoMesh * GenerateGmesh(int nx, int ny, double l, double h);

//Stablish de force fuction for 1D and 2D mesh
void Ladoderecho_1D(const TPZVec<REAL> &pt, TPZVec<STATE> &disp);
void Ladoderecho_2D(const TPZVec<REAL> &pt, TPZVec<STATE> &disp);

//Stablish an exact solution
void SolExact(const TPZVec<REAL> &ptx, TPZVec<STATE> &sol, TPZFMatrix<STATE> &flux);

//Creating computational meshes
TPZCompMesh * GeneratePressureCmesh(TPZGeoMesh *Gmesh, int order_internal);
TPZCompMesh * GenerateConstantCmesh(TPZGeoMesh *Gmesh, bool third_LM);
TPZCompMesh * GenerateFluxCmesh(TPZGeoMesh *Gmesh, int order_internal, int order_border);
TPZMultiphysicsCompMesh * GenerateMixedCmesh(TPZVec<TPZCompMesh *> fvecmesh, int order, bool two_d_Q);

//Creates index vector
void IndexVectorCoFi(TPZMultiphysicsCompMesh *Coarse_sol, TPZMultiphysicsCompMesh *Fine_sol, TPZVec<int64_t> & indexvec);

//Hdiv Test
void HDiv(int nx, int order_small, int order_high, bool condense_equations_Q, bool two_d_Q);


//Transfer DOF from coarse mesh to fine mesh
void TransferDegreeOfFreedom(TPZFMatrix<STATE> & CoarseDoF, TPZFMatrix<STATE> & FineDoF, TPZVec<int64_t> & DoFIndexes);

//Analysis configuration
void ConfigurateAnalyses(TPZCompMesh * cmesh_c, TPZCompMesh * cmesh_f, bool must_opt_band_width_Q, int number_threads, TPZAnalysis *an_c,TPZAnalysis *an_f, bool UsePardiso_Q);
void SplitConnects(TPZCompMesh *fluxmesh, TPZGeoEl *gel ,int j);
void ShowShape(TPZCompMesh * cmesh, int element, int funcion,std::string plotname);
using namespace std;



int main(){
    
#ifdef LOG4CXX
    InitializePZLOG();
#endif
    
    
    HDiv(5, 1, 4, true, true);

    
    
}

/**
 * @brief Runs a HDiv problem with 4 spaces for 1D or 2D case
 * @param order_small: Low order for internal elements
 * @param order_high: High order for border elements
 * @param condense_equations_Q: Bool wether the problem is condensed or not
 * @param two_d_Q: Bool wether the problem es 1D (false) or 2D (true)
 */
void HDiv(int nx, int order_small, int order_high, bool condense_equations_Q, bool two_d_Q){
   
//    TPZGeoMesh *gmesh_11 = GenerateGmesh(nx, nx, 2, 2);      // Generates a 2D geo mesh
//    TPZCompMesh *flux = GenerateFluxCmesh(gmesh_11, 1, 1);
//
//    int el_index=3;
//    int nfun=flux->Element(el_index)->NEquations();
//    for (int i=0; i<nfun; i++) {
//
//        std::string filename("elementFunc.vtk");
//        std::string file(filename+std::to_string(i)+".vtk");
//        ShowShape(flux,el_index,i,file);
//
//
//    }

    bool KeepOneLagrangian = false;
    bool KeepMatrix = false;
    bool render_shapes_Q = false;                      //Prints a VTK showing the render shapes
    bool must_opt_band_width_Q = true;
    int number_threads = 0;
    
    TPZGeoMesh *gmesh;
    
    TPZGeoMesh *gmesh_1 = GenerateGmesh(nx, nx, 1, 1);      // Generates a 2D geo mesh
    TPZGeoMesh *gmesh_2 = GenerateGmeshOne(nx, 1);          // Generates a 1D geo mesh
    
    if (two_d_Q) {                  //Asks if the problem is 2D
        gmesh = gmesh_1;
    } else {
        gmesh = gmesh_2;
    }
    
    TPZMultiphysicsCompMesh *MixedMesh_coarse = 0;
    TPZManVector<TPZCompMesh *> vecmesh_c(4);      //Vector for coarse mesh case (4 spaces)
    {
        TPZCompMesh *q_cmesh = GenerateFluxCmesh(gmesh, order_high, order_small);
        TPZCompMesh *p_cmesh = GeneratePressureCmesh(gmesh, order_high);
        TPZCompMesh *gavg_cmesh = GenerateConstantCmesh(gmesh,false);
        TPZCompMesh *pavg_cmesh = GenerateConstantCmesh(gmesh,true);
        vecmesh_c[0] = q_cmesh;              //Flux
        vecmesh_c[1] = p_cmesh;              //Pressure
        vecmesh_c[2] = gavg_cmesh;           //Average distribute flux
        vecmesh_c[3] = pavg_cmesh;           //Average pressure
        
        MixedMesh_coarse = GenerateMixedCmesh(vecmesh_c, 1, two_d_Q);       //1 Stands for the corse mesh order
    }
    
    //
   
    //
    
    
    if (condense_equations_Q) {             //Asks if you want to condesate the problem
        MixedMesh_coarse->ComputeNodElCon();
        int dim = MixedMesh_coarse->Dimension();
        int64_t nel = MixedMesh_coarse->NElements();
        for (int64_t el =0; el<nel; el++) {
            TPZCompEl *cel = MixedMesh_coarse->Element(el);
            if(!cel) continue;
            TPZGeoEl *gel = cel->Reference();
            if(!gel) continue;
            if(gel->Dimension() != dim) continue;
            int nc = cel->NConnects();
            cel->Connect(nc-1).IncrementElConnected();
        }
        // Created condensed elements for the elements that have internal nodes
        TPZCompMeshTools::CreatedCondensedElements(MixedMesh_coarse, KeepOneLagrangian, KeepMatrix);
    }
    
    TPZMultiphysicsCompMesh * MixedMesh_fine = 0;
    TPZManVector<TPZCompMesh *> vecmesh_f(4);      //Vector fine mesh case
    {
        
        TPZCompMesh *q_cmesh = GenerateFluxCmesh(gmesh, order_high, order_high);
        TPZCompMesh *p_cmesh = GeneratePressureCmesh(gmesh, order_high);
        TPZCompMesh *gavg_cmesh = GenerateConstantCmesh(gmesh,false);
        TPZCompMesh *pavg_cmesh = GenerateConstantCmesh(gmesh,true);
        vecmesh_f[0] = q_cmesh;              //Flux
        vecmesh_f[1] = p_cmesh;              //Pressure
        vecmesh_f[2] = gavg_cmesh;           //Average distribute flux
        vecmesh_f[3] = pavg_cmesh;           //Average pressure
        
        MixedMesh_fine = GenerateMixedCmesh(vecmesh_f, 2, two_d_Q);;       //2 Stands for the corse mesh order
    }
    
    
    if (condense_equations_Q) {             //Asks if you want to condesate the problem
        
        MixedMesh_fine->ComputeNodElCon();
        int dim = MixedMesh_fine->Dimension();
        int64_t nel = MixedMesh_fine->NElements();
        for (int64_t el =0; el<nel; el++) {
            TPZCompEl *cel = MixedMesh_fine->Element(el);
            if(!cel) continue;
            TPZGeoEl *gel = cel->Reference();
            if(!gel) continue;
            if(gel->Dimension() != dim) continue;
            int nc = cel->NConnects();
            cel->Connect(nc-1).IncrementElConnected();
        }
        
        // Created condensed elements for the elements that have internal nodes
        TPZCompMeshTools::CreatedCondensedElements(MixedMesh_fine, KeepOneLagrangian, KeepMatrix);
    }
    
    //Solving the system:
    MixedMesh_coarse->InitializeBlock();        //Resequence the block object, remove unconnected connect objects
    MixedMesh_fine->InitializeBlock();          //and reset the dimension of the solution vector
    TPZAnalysis *an_c = new TPZAnalysis;
    TPZAnalysis *an_f = new TPZAnalysis;
    ConfigurateAnalyses(MixedMesh_coarse, MixedMesh_fine, must_opt_band_width_Q, number_threads, an_c, an_f, true); //True to use pardiso
    

    
//    if(render_shapes_Q){
//        TPZAnalysis anloc(MixedMesh_coarse,false);
//        std::string filename("Shape.vtk");
//        TPZVec<int64_t> indices(20);
//        for(int i=0; i<20; i++) indices[i] = i;
//        const TPZVec<std::string> varname(1);
//        varname[0]="Flux";
//anloc.ShowShape(filename, indices,1,varname);
//
//    }
    
    // Solving and postprocessing problems separately
  
    
    // Resolver coarse
    an_c->Assemble();

    an_f->Assemble();
    
    an_c->Solve();
    
    an_f->Solve();
    // An iterative solution
    {
        
        // constructing block diagonal.
        if(1){
            TPZBlockDiagonalStructMatrix bdstr(MixedMesh_fine);     //Give the fine mesh
            TPZBlockDiagonal<STATE> * sp = new TPZBlockDiagonal<STATE>();
            bdstr.AssembleBlockDiagonal(*sp);

            TPZAutoPointer<TPZMatrix<STATE> > sp_auto(sp);
            int64_t n_con = MixedMesh_fine->NConnects();
            for (int ic = 0; ic < n_con; ic++) {
                TPZConnect & con = MixedMesh_fine->ConnectVec()[ic];
                bool check = con.IsCondensed() || con.HasDependency() || con.LagrangeMultiplier() ==0;
                if (check) {
                    continue;
                }

                int64_t seqnum = con.SequenceNumber();
                int block_size = MixedMesh_fine->Block().Size(seqnum);
                if (block_size != 1) {
                    continue;
                }

                int64_t pos = MixedMesh_fine->Block().Position(seqnum);
                (*sp).PutVal(pos, pos, 1.0);
            }

            
            TPZVec<int64_t> Indexes;
            IndexVectorCoFi(MixedMesh_coarse, MixedMesh_fine, Indexes);
            int64_t neq_coarse = MixedMesh_coarse->NEquations();
            int64_t neq_fine = MixedMesh_fine->NEquations();
            TPZHdivTransfer<STATE> *transfer = new TPZHdivTransfer<STATE>(neq_coarse, neq_fine, Indexes);
            TPZFMatrix<STATE> coarsesol(neq_coarse,1,1.), finesol(neq_fine,1,1.);
            transfer->Multiply(finesol, coarsesol,0); //It mutiplies itself by TPZMatrix<TVar>A putting the result in res
            transfer->Multiply(coarsesol, finesol,1); //z = beta * y(coarse) + alpha * opt(this)*x (fine)

            finesol.Print(std::cout);
            coarsesol.Print(std::cout);
            
            
            //Transfers the solution from coarse to fine mesh
            TPZStepSolver<STATE> step;
            step.SetDirect(ELDLt);
            an_c->Solver().Solve(coarsesol,coarsesol);

            TPZMGSolver<STATE> mgsolve(transfer,an_c->Solver(),1);
            mgsolve.SetMatrix(an_f->Solver().Matrix());
            mgsolve.Solve(finesol, finesol);
            
            //End of transferation
            
            finesol.Print(std::cout);
            coarsesol.Print(std::cout);
            
            //??
            TPZFMatrix<STATE> rhscoarse = an_c->Rhs();
            TPZFMatrix<STATE> rhsfine = an_f->Rhs();
            rhsfine.Print("rhsfine" , std::cout);
            TPZStepSolver<STATE> BDSolve(sp_auto);
            BDSolve.SetDirect(ELU);
            TPZSequenceSolver<STATE> seqsolver;

            seqsolver.SetMatrix(an_f->Solver().Matrix());
            seqsolver.AppendSolver(mgsolve); //Updates the values of the preconditioner based on the values of the matrix
            seqsolver.AppendSolver(BDSolve);
            seqsolver.AppendSolver(mgsolve);
            seqsolver.Solve(rhsfine, finesol);
            
            finesol.Print(std::cout);
            coarsesol.Print(std::cout);

            std::ofstream file("matblock.nb");
            sp->Print("k = ",file,EMathematicaInput);
            TPZStepSolver<STATE> cg_solve(an_f->Solver().Matrix());
            cg_solve.SetCG(10, seqsolver, 1.e-6, 0);
            finesol.Zero();
            cg_solve.Solve(rhsfine, finesol);




        }
     if(1){
            

          
            TPZBuildMultiphysicsMesh::TransferFromMultiPhysics(vecmesh_c, MixedMesh_coarse);
            TPZBuildMultiphysicsMesh::TransferFromMultiPhysics(vecmesh_f, MixedMesh_fine);

            //PostProcess
            TPZStack<std::string> scalar, vectors;
            TPZManVector<std::string,10> scalnames(4), vecnames(1);
            vecnames[0]  = "q";
            scalnames[0] = "p";
            scalnames[1] = "kappa";
            scalnames[1] = "div_q";
            scalnames[2] = "g_average";
            scalnames[3] = "u_average";
            
            std::ofstream filePrint_coarse("MixedHdiv_coarse.txt");
            MixedMesh_coarse->Print(filePrint_coarse);
            std::string name_coarse = "MixedHdiv_coarse.vtk";
            
            
            std::ofstream filePrint_fine("MixedHdiv_fine.txt");
            MixedMesh_fine->Print(filePrint_fine);
            std::string name_fine = "MixedHdiv_fine.vtk";
            
            int di;
            if (two_d_Q) {
                di = 2;                 //Dimension definition
            } else {
                di = 1;                 //Dimension definition
            }
            an_c->DefineGraphMesh(di, scalnames, vecnames, name_coarse);
            an_c->PostProcess(3,di);
           
            an_f->DefineGraphMesh(di, scalnames, vecnames, name_fine);
            an_f->PostProcess(3,di);
        }
        
    }
    
    
}

/**
 * @brief Generates the geometric mesh
 * @param nx: number of partions on x
 * @param ny: number of partions on y
 * @param l: lenght
 * @param h: height
 * @return Geometric mesh
 */
TPZGeoMesh * GenerateGmesh(int nx, int ny, double l, double h){
    
    TPZGeoMesh *gmesh = new TPZGeoMesh;
    
    TPZVec<int> nels(3,0);
    nels[0]=nx;         //Elements over x
    nels[1]=ny;         //Elements over y
    
    TPZVec<REAL> x0(3,0.0);
    TPZVec<REAL> x1(3,l);
    x1[1]=h;
    x1[2]=0;
    
    //Setting boundary conditions (negative numbers to recognize them)
    TPZGenGrid gen(nels,x0,x1);
    gen.SetElementType(EQuadrilateral);
    gen.Read(gmesh);
    gen.SetBC(gmesh, 4, -1);
    gen.SetBC(gmesh, 5, -2);
    gen.SetBC(gmesh, 6, -3);
    gen.SetBC(gmesh, 7, -4);
    return gmesh;
}

/**
 * @brief Generates the pressure computational mesh
 * @param Geometric mesh
 * @param Order
 * @return Pressure computational mesh
 */
TPZCompMesh * GeneratePressureCmesh(TPZGeoMesh *Gmesh, int order){
    
    TPZCompMesh *Cmesh= new TPZCompMesh (Gmesh);
    
    Cmesh->SetDimModel(Gmesh->Dimension());
    Cmesh->SetDefaultOrder(order);
    Cmesh->SetAllCreateFunctionsDiscontinuous();
    Cmesh->ApproxSpace().CreateDisconnectedElements(true);
    
    //Add material to the mesh
    int dimen = Gmesh->Dimension();
    int MaterialId = 1;
    STATE Permeability=1;
    
    TPZMatPoisson3d *mat = new TPZMatPoisson3d(MaterialId, dimen);
    
    //No convection
    REAL conv=0;
    TPZVec<REAL> convdir(dimen, 0);
    mat->SetParameters(Permeability, conv, convdir);
    
    //Insert material to mesh
    Cmesh->InsertMaterialObject(mat);
    
    //Autobuild
    Cmesh->AutoBuild();
    
    int ncon = Cmesh->NConnects();
    
    //Set Lagrange multiplier
    for(int i=0; i<ncon; i++){
        TPZConnect &newnod = Cmesh->ConnectVec()[i];
        newnod.SetLagrangeMultiplier(1);
    }
    return Cmesh;
}

/**
 * @brief Generates the constant computational mesh
 * @param Gmesh: Geometric mesh
 * @param third_LM: Bool Third Lagrange multiplier
 * @return Constant computational mesh
 */
TPZCompMesh * GenerateConstantCmesh(TPZGeoMesh *Gmesh, bool third_LM)
{
    TPZCompMesh *Cmesh= new TPZCompMesh (Gmesh);
    
    Cmesh->SetDimModel(Gmesh->Dimension());
    Cmesh->SetDefaultOrder(0);
    Cmesh->SetAllCreateFunctionsDiscontinuous();

    //Add material to the mesh
    int dimen = Gmesh->Dimension();
    int MaterialId = 1;
    
    TPZNullMaterial *mat =new TPZNullMaterial(MaterialId);
    mat->SetDimension(dimen);
    mat->SetNStateVariables(1);

    //Insert material to mesh
    Cmesh->InsertMaterialObject(mat);
    
    //Autobuild
    Cmesh->AutoBuild();
    
    int ncon = Cmesh->NConnects();
    for(int i=0; i<ncon; i++)
    {
        TPZConnect &newnod = Cmesh->ConnectVec()[i];
        if (third_LM) {
            newnod.SetLagrangeMultiplier(3);
        }else{
            newnod.SetLagrangeMultiplier(2);
        }
    }
    return Cmesh;
}


/**
 * @brief Generates the flux computational mesh
 * @param mesh: Geometric mesh
 * @param order_internal: Order used for internal elements
 * @param order_border: Order used for border elements
 * @return Flux computational mesh
 */
TPZCompMesh * GenerateFluxCmesh(TPZGeoMesh *mesh, int order_internal, int order_border){
    
    int dimen = mesh->Dimension();
    TPZCompMesh *Cmesh = new TPZCompMesh(mesh);
    Cmesh->SetDimModel(dimen);
    Cmesh->SetDefaultOrder(order_border);
    
    //Definition of the approximation space
    int perm=1;
    REAL conv=0;
    REAL perme=1;
    TPZVec<REAL> convdir(dimen , 0.0);
    TPZMatPoisson3d *mat = new TPZMatPoisson3d(perm , dimen);
    mat->SetParameters(perme, conv, convdir);
    
    //Inserting volumetric materials objects
    Cmesh->InsertMaterialObject(mat);
    
    //Create H(div) functions
 
    Cmesh->SetAllCreateFunctionsHDiv();
    
    //Insert boundary conditions
    int D=0;
    int BC1=-1;
    TPZFMatrix<STATE> val1(1,1,0.0);
    TPZFMatrix<STATE> val2(1,1,0.0);
    TPZMaterial *bc1 = mat->CreateBC(mat, BC1, D, val1, val2);
    Cmesh->InsertMaterialObject(bc1);

    int BC2=-2;
    val2(0,0)=0;
    TPZMaterial *bc2 = mat->CreateBC(mat, BC2, D, val1, val2);
    Cmesh->InsertMaterialObject(bc2);

    int BC3=-3;
    val2(0,0)=0;
    TPZMaterial *bc3 = mat->CreateBC(mat, BC3, D, val1, val2);
    Cmesh->InsertMaterialObject(bc3);

    int BC4=-4;
    val2(0,0)=0;
    TPZMaterial *bc4 = mat->CreateBC(mat, BC4, D, val1, val2);
    Cmesh->InsertMaterialObject(bc4);
    

    Cmesh->AutoBuild();
    
    int64_t nel = Cmesh->NElements();
    for (int el=0; el<nel; el++) {
        TPZCompEl *cel = Cmesh->Element(el);
        if(!cel) continue;
        TPZInterpolatedElement *intel = dynamic_cast<TPZInterpolatedElement *>(cel);
        if(!intel) DebugStop();
        TPZGeoEl *gel = intel->Reference();
        intel->SetSideOrder(gel->NSides()-1, order_internal);
    }
    Cmesh->ExpandSolution();
    
    return Cmesh;
}

/**
 * @brief Generates the mixed computational mesh
 * @param fvecmesh: Vector thats contains flux and pressure computational mesh
 * @param order: Ordertwo_d_Q
 * @param two_d_Q: Bool wether is 1D (false) or 2D (true)
 * @return Mixed computational mesh
 */

TPZMultiphysicsCompMesh * GenerateMixedCmesh(TPZVec<TPZCompMesh *> fvecmesh, int order, bool two_d_Q){
    TPZGeoMesh *gmesh = fvecmesh[1]->Reference();
    TPZMultiphysicsCompMesh *MixedMesh = new TPZMultiphysicsCompMesh(gmesh);
    
    //Definition of the approximation space
    int dimen= gmesh->Dimension();
    int matnum=1;
    REAL perm=1;
    
    //Inserting material
    TPZMixedDarcyWithFourSpaces * mat = new TPZMixedDarcyWithFourSpaces(matnum, dimen);
    mat->SetPermeability(perm);
    
    if (two_d_Q) {
        TPZAutoPointer<TPZFunction<STATE> > sourceterm = new TPZDummyFunction<STATE>(Ladoderecho_2D, 10);
        mat->SetForcingFunction(sourceterm);
    } else {
        TPZAutoPointer<TPZFunction<STATE> > sourceterm = new TPZDummyFunction<STATE>(Ladoderecho_1D, 10);
        mat->SetForcingFunction(sourceterm);
    }
    
    //Inserting volumetric materials objects
    MixedMesh->InsertMaterialObject(mat);
    
    //Boundary conditions
    int D=0;
    int BC1=-1;
    TPZFMatrix<STATE> val1(1,1,0.0);
    TPZFMatrix<STATE> val2(1,1,0.0);
    
    val2(0,0)=0.0;
    TPZMaterial *bc1 = mat->CreateBC(mat, BC1, D, val1, val2);
    MixedMesh->InsertMaterialObject(bc1);
    
    int BC2=-2;
    val2(0,0)=0;
    TPZMaterial *bc2 = mat->CreateBC(mat, BC2, D, val1, val2);
    MixedMesh->InsertMaterialObject(bc2);
    
    int BC3=-3;
    val2(0,0)=0;
    TPZMaterial *bc3 = mat->CreateBC(mat, BC3, D, val1, val2);
    MixedMesh->InsertMaterialObject(bc3);

    int BC4=-4;
    val2(0,0)=0;
    TPZMaterial *bc4 = mat->CreateBC(mat, BC4, D, val1, val2);
    MixedMesh->InsertMaterialObject(bc4);
    
    MixedMesh->SetAllCreateFunctionsMultiphysicElem();
    MixedMesh->SetDimModel(dimen);
    
    //Autobuild
    TPZManVector<int,5> active_approx_spaces(4); /// 1 stands for an active approximation spaces
    active_approx_spaces[0] = 1;
    active_approx_spaces[1] = 1;
    active_approx_spaces[2] = 1;
    active_approx_spaces[3] = 1;
    MixedMesh->BuildMultiphysicsSpace(active_approx_spaces,fvecmesh);
    
    TPZBuildMultiphysicsMesh::AddElements(fvecmesh, MixedMesh);
    TPZBuildMultiphysicsMesh::AddConnects(fvecmesh,MixedMesh);
    TPZBuildMultiphysicsMesh::TransferFromMeshes(fvecmesh, MixedMesh);
    
    std::cout<<"n equ: "<<MixedMesh->NEquations()<<std::endl;
    std::cout<<"n equ: "<<fvecmesh[0]->NEquations()<<std::endl;
    std::cout<<"n equ: "<<fvecmesh[1]->NEquations()<<std::endl;
    std::cout<<"n equ: "<<fvecmesh[2]->NEquations()<<std::endl;
    std::cout<<"n equ: "<<fvecmesh[3]->NEquations()<<std::endl;
    std::cout<<"------------------------------"<<std::endl;
    return MixedMesh;
};

/**
 * @brief Generates the force function for the 1D case
 * @param pt: Points values
 * @param disp: Vector
 */
void Ladoderecho_1D (const TPZVec<REAL> &pt, TPZVec<STATE> &disp){
    
    STATE x = pt[0];
    double fx= 4*M_PI*M_PI*sin(2*M_PI*x);        //Force function definition
    disp[0]=fx;
    
}

/**
 * @brief Generates the force function for the 2D case
 * @param pt: Points values
 * @param disp: Vector
 */

void Ladoderecho_2D (const TPZVec<REAL> &pt, TPZVec<STATE> &disp){
    
    STATE x = pt[0];
    STATE y = pt[1];
    double fx= 2*M_PI*M_PI*sin(M_PI*x)*sin(M_PI*y);        //Force function definition
    disp[0]=fx;
    
}

/**
 * @brief Generates a index vector which relates coarse and fine mesh
 * @param Coarse_sol: Coarse mesh
 * @param Fine_sol: Fine mesh
 * @param indexvec:
 */
void IndexVectorCoFi(TPZMultiphysicsCompMesh *Coarse_sol, TPZMultiphysicsCompMesh *Fine_sol, TPZVec<int64_t> & indexvec)
{
    int64_t maxcone = Coarse_sol->NConnects();
    
    int64_t indexvecsize = 0;
    for (int j=0; j<maxcone; j++) {
        bool is_condensed = Coarse_sol->ConnectVec()[j].IsCondensed();
        if (is_condensed == true ) continue;

        int64_t sequence_coarse = Coarse_sol->ConnectVec()[j].SequenceNumber();
        int blocksize_coarse = Coarse_sol->Block().Size(sequence_coarse);

        indexvecsize += blocksize_coarse;
    }
    indexvec.Resize(indexvecsize,-1);
    
    for (int j=0; j<maxcone; j++) {
        bool is_condensed = Coarse_sol->ConnectVec()[j].IsCondensed();
        if (is_condensed == true ) continue;

        int64_t sequence_coarse = Coarse_sol->ConnectVec()[j].SequenceNumber();
        int64_t sequence_fine = Fine_sol->ConnectVec()[j].SequenceNumber();
        int blocksize_coarse = Coarse_sol->Block().Size(sequence_coarse);
        int blocksize_fine = Fine_sol->Block().Size(sequence_fine);

        if (blocksize_coarse > blocksize_fine) {
            DebugStop();

        }
    
        for(int i=0; i<blocksize_coarse; i++){
            int64_t pos_coarse = Coarse_sol->Block().Position(sequence_coarse);
            int64_t pos_fine = Fine_sol->Block().Position(sequence_fine);
            indexvec[pos_coarse+i] = pos_fine+i;
        }
    }
}

/**
 * @brief Transfer DOF from coarse to fine mesh
 * @param CoarseDoF: Solution coarse matrix
 * @param FineDoF: Solution fine matrix
 * @param DoFIndexes: DOF index vector
 */
void TransferDegreeOfFreedom(TPZFMatrix<STATE> & CoarseDoF, TPZFMatrix<STATE> & FineDoF, TPZVec<int64_t> & DoFIndexes){
    
    int64_t n_data = DoFIndexes.size();
    for (int64_t i = 0 ; i < n_data; i++) {
        FineDoF(DoFIndexes[i],0) = CoarseDoF(i,0);
    }

}

/**
 * @brief Generates a geometric 1D mesh
 * @param nx: number of partions on x
 * @param l: lenght
 * @return Geometric 1D mesh
 */
TPZGeoMesh * GenerateGmeshOne(int nx, double l){
    //Creates vector nodes
    double h = l/nx;
    int Domain_Mat_Id = 1;
    int Inlet_bc_Id = -1;
    int Outletbc_Id = -2;
    TPZVec<REAL> xp(3,0.0);
//    cout << xp;
    TPZGeoMesh *gmesh = new TPZGeoMesh;
    gmesh->NodeVec().Resize(nx+1);
//    gmesh->NodeVec().NElements();
    for (int64_t i=0; i<nx+1; i++) {
        xp[0] =(i)*h;
        gmesh->NodeVec()[i]= TPZGeoNode(i, xp, *gmesh);
    }

//    for (auto &item: gmesh->NodeVec()) {
//        item.Print();
//    }

    //Creates elements
    TPZVec<int64_t> cornerindexes(2);
    for (int64_t iel=0; iel<nx; iel++) {
        cornerindexes[0]=iel;
        cornerindexes[1]=iel+1;
        gmesh->CreateGeoElement(EOned, cornerindexes, Domain_Mat_Id, iel);
    }
    //Set BC
    gmesh->Element(0)->CreateBCGeoEl(0, Inlet_bc_Id);
    gmesh->Element(nx-1)->CreateBCGeoEl(1, Outletbc_Id);
    gmesh->SetDimension(1);
    gmesh->BuildConnectivity();
    
    return gmesh;
}

/**
 * @brief Configurate the matrix for analysis
 * @param cmesh_c: Computational coarse mesh
 * @param cmesh_f: Computational fine mesh
 * @param must_opt_band_width_Q: Wether the band width is optimized or not (it is neccesaty to rearrange the matrix in order to be less sparse)
 * @param number_threads: 
 * @param an_c: Coarse analysis mesh
 * @param an_f: Fine analysis mesh
 * @param UsePardiso_Q: Wether using Pardiso or not
 */
void ConfigurateAnalyses(TPZCompMesh * cmesh_c, TPZCompMesh * cmesh_f, bool must_opt_band_width_Q, int number_threads, TPZAnalysis *an_c,TPZAnalysis *an_f, bool UsePardiso_Q){
    
        an_c->SetCompMesh(cmesh_c,must_opt_band_width_Q);
        an_f->SetCompMesh(cmesh_f,must_opt_band_width_Q);
        TPZStepSolver<STATE> step;
        if (UsePardiso_Q) {
    
            TPZSymetricSpStructMatrix sparse_matrix_coarse(cmesh_c);
            TPZSymetricSpStructMatrix sparse_matrix_fine(cmesh_f);
            sparse_matrix_coarse.SetNumThreads(number_threads);
            sparse_matrix_fine.SetNumThreads(number_threads);
            an_c->SetStructuralMatrix(sparse_matrix_coarse);
            an_f->SetStructuralMatrix(sparse_matrix_fine);
    
        }else{
    
            TPZSkylineStructMatrix sparse_matrix_coarse(cmesh_c);
            TPZSkylineStructMatrix sparse_matrix_fine(cmesh_f);
            sparse_matrix_coarse.SetNumThreads(number_threads);
            sparse_matrix_fine.SetNumThreads(number_threads);
            an_c->SetStructuralMatrix(sparse_matrix_coarse);
            an_f->SetStructuralMatrix(sparse_matrix_fine);
    
        }
        step.SetDirect(ELDLt);
        an_c->SetSolver(step);
        an_f->SetSolver(step);
    }
void ShowShape(TPZCompMesh * cmesh, int element, int funcion,std::string plotfile){
    TPZGeoMesh *gmesh = cmesh->Reference();
    gmesh->ResetReference();
    
    int nels = cmesh->NElements();
    for (int iel =0 ; iel<nels; iel++) {
        TPZCompEl *cel = cmesh->Element(iel);
        TPZGeoEl *gel = cel->Reference();
        if (gel->Index() == element) {
            gel->SetMaterialId(100);
        }
    }
    
    TPZMaterial  * mat_2(cmesh->MaterialVec()[1]);
    std::map<int, TPZMaterial *> matvec;
    mat_2->Clone(matvec);
    cmesh->CleanUp();
 
    TPZMatPoisson3d *mat = new TPZMatPoisson3d(100 , 2);
    TPZMaterial *perf_mat( matvec[1]);
    TPZMatPoisson3d *aux_mat = dynamic_cast<TPZMatPoisson3d *>(perf_mat);
    
    cmesh->InsertMaterialObject(mat);
    cmesh->AutoBuild();
  
    int gdl = cmesh->Solution().Rows();
    int nfuncols = cmesh->Solution().Cols();
    int nfunrows = cmesh->Solution().Rows();
    TPZFMatrix<STATE> solu(nfunrows,nfuncols,1.0);
    TPZFMatrix<STATE> sol(gdl,1,0.0);
  
    sol.Resize(gdl, 1);
    int nel = cmesh->NElements();
    int val =1;
    for (int i=0; i<nel; i++) {
        TPZCompEl *cel = cmesh->Element(i);
        TPZGeoEl *gel =cel->Reference();
        int index = gel->Index();
        if (index != element) {
            continue;
        }

        int nconnects = cel->NConnects();
        int acum =0;
       
        for(int j=0; j<nconnects; j++){
            int sec_number = cel->Connect(j).fSequenceNumber;
            if(sec_number > -1)
            {
                acum = acum + cmesh->Block().Size(sec_number);
                if (acum > funcion) {

//                    if (val) {
                    
                        //SplitConnects(cmesh, gel ,j);
//                        cmesh->CleanUp();
                        val=0;
                        
                        int delta = funcion + cmesh->Block().Size(sec_number) - acum;
                        int64_t pos = cmesh->Block().Position(sec_number);
                        pos = pos +  delta;
                        sol.Zero();
                        sol(pos,0)=1.0;
                        cmesh->LoadSolution(sol);
//                        if (funcion == 2) {
//                            sol.Print(std::cout);
//                        }
                        TPZAnalysis *an = new TPZAnalysis(cmesh,false);
                        {
                            const int dim = an->Mesh()->Dimension();
                            int div = 5;
                            // std::string plotfile = "SHAPES.vtk";
                            TPZStack<std::string> scalar_names, vec_names;
                            
                            scalar_names.push_back("Solution");
                            an->DefineGraphMesh(dim,scalar_names,vec_names,plotfile);
                            an->PostProcess(div,dim);
                            std::cout << "The function :"<<funcion<<" of element "<<element<<" has been printed on .vtk file" << std::endl;
                            
                        }
                        int nelss = gmesh->NElements();
                        for (int iel =0 ; iel<nelss; iel++) {
                            TPZGeoEl *gel = gmesh->Element(iel);
                            if (gel->Index() == element) {
                                gel->SetMaterialId(1);
                            }
//                        }
                        cmesh->CleanUp();
                        cmesh->InsertMaterialObject(aux_mat);
                        cmesh->AutoBuild();
                        
                        return;

                    }
                }
            }
        }
    }
    
    //
    
}
void SplitConnects(TPZCompMesh *fluxmesh, TPZGeoEl *gel , int j){
    //j -> connect index
    TPZCompEl *cel = gel->Reference();
    
    int nelconnect = cel->Connect(j).NElConnected();
    if (nelconnect>1) {
        
    }
   
  
//    for (int iside=4; iside<cel->Reference()->NSides()-1; iside++) {
        int iside = j + 4;
    if (iside >= gel->NSides()-1) {
        return;
    }
        TPZGeoElSide neighb(gel, iside);
        TPZStack<TPZGeoElSide> allneigh;
        TPZGeoElSide neig = neighb.Neighbour();
        if (neig.Element()->Dimension() !=2) {
            return;
        }
    
        TPZCompElSide left(cel,iside);
        
     //   TPZGeoEl *gel = left.Element()->Reference();
        TPZGeoElSide gelr = gel->Neighbour(iside);
        TPZCompElSide right(gelr.Element()->Reference(), gelr.Side());
        
        //TPZCompMesh *pressuremesh = meshvec[1];
        //TPZGeoMesh *gmesh = fluxmesh->Reference();
        TPZGeoElSide gleft(gel,iside);
        TPZGeoElSide gright(gleft.Neighbour());
        TPZInterpolatedElement *intelleft = dynamic_cast<TPZInterpolatedElement *> (left.Element());
        TPZInterpolatedElement *intelright = dynamic_cast<TPZInterpolatedElement *> (right.Element());
        intelleft->SetSideOrient(left.Side(), 1);
        intelright->SetSideOrient(right.Side(), 1);
        TPZStack<TPZCompElSide> equalright;
        TPZConnect &cleft = intelleft->SideConnect(0, left.Side());
        int conr = right.Element()->Connect(right.Side()-4).SequenceNumber();
       int conl = left.Element()->Connect(left.Side()-4).SequenceNumber();
    
    if (conr != conl) {
        return;
    }
    
        
        int64_t index = fluxmesh->AllocateNewConnect(cleft);
        TPZConnect &newcon = fluxmesh->ConnectVec()[index];
        cleft.DecrementElConnected();
        newcon.ResetElConnected();
        newcon.IncrementElConnected();
        newcon.SetSequenceNumber(fluxmesh->NConnects() - 1);
        
        int rightlocindex = intelright->SideConnectLocId(0, right.Side());
        intelright->SetConnectIndex(rightlocindex, index);
        
        int sideorder = cleft.Order();
        fluxmesh->SetDefaultOrder(sideorder);
        
        int gdl = fluxmesh->Solution().Rows();
        fluxmesh->Solution().Resize(gdl+2, 1);
        TPZFMatrix<STATE> sol(gdl+2,1,0.0);
        fluxmesh->LoadSolution(sol);
    
//    }
    
    
    
    
//    std::string filename("elementFunc_new.vtk");
//    std::string file(filename+std::to_string(i)+".vtk");
//    std::ofstream filename5(filename+std::to_string(i)+".txt");
//    fluxmesh->Print(filename5);
    return;
}
