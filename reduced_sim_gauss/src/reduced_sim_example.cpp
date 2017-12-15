#include <vector>

//#include <Qt3DIncludes.h>
#include <GaussIncludes.h>
#include <ForceSpring.h>
#include <FEMIncludes.h>
#include <PhysicalSystemParticles.h>
//Any extra things I need such as constraints
#include <ConstraintFixedPoint.h>
#include <TimeStepperEulerImplicitLinear.h>

#include <igl/writeDMAT.h>
#include <igl/viewer/Viewer.h>
#include <igl/readMESH.h>
#include <igl/unproject_onto_mesh.h>

#include <stdlib.h>
#include <iostream>
#include <string>
#include <sstream>

// Optimization
#include <LBFGS.h>

using namespace Gauss;
using namespace FEM;
using namespace ParticleSystem; //For Force Spring

/* Tetrahedral finite elements */

//typedef physical entities I need

typedef PhysicalSystemFEM<double, NeohookeanTet> NeohookeanTets;

typedef World<double, 
                        std::tuple<PhysicalSystemParticleSingle<double> *, NeohookeanTets *>,
                        std::tuple<ForceSpringFEMParticle<double> *>,
                        std::tuple<ConstraintFixedPoint<double> *> > MyWorld;
typedef TimeStepperEulerImplictLinear<double, AssemblerEigenSparseMatrix<double>,
AssemblerEigenVector<double> > MyTimeStepper;

// Mesh
Eigen::MatrixXd V; // Verts
Eigen::MatrixXi T; // Tet indices
Eigen::MatrixXi F; // Face indices

// Mouse/Viewer state
Eigen::RowVector3f last_mouse;
Eigen::RowVector3d dragged_pos;
bool is_dragging = false;
int dragged_vert = 0;
int current_frame = 0;

// Parameters
bool saving_training_data = false;
std::string output_dir = "output_data/";


void save_displacements_DMAT(const std::string path, MyWorld &world, NeohookeanTets *tets) { // TODO: Add mouse position data to ouput
    auto q = mapDOFEigen(tets->getQ(), world);
    Eigen::Map<Eigen::MatrixXd> dV(q.data(), V.cols(), V.rows()); // Get displacements only
    Eigen::MatrixXd displacements = dV.transpose();

    igl::writeDMAT(path, displacements, false); // Don't use ascii
}

void save_base_configurations_DMAT(Eigen::MatrixXd &V, Eigen::MatrixXi &F) {
    std::stringstream verts_filename, faces_filename;
    verts_filename << output_dir << "base_verts.dmat";
    faces_filename << output_dir << "base_faces.dmat";
    
    igl::writeDMAT(verts_filename.str(), V, false); // Don't use ascii
    igl::writeDMAT(faces_filename.str(), F, false); // Don't use ascii
}


// Todo put this in utilities
Eigen::MatrixXd getCurrentVertPositions(MyWorld &world, NeohookeanTets *tets) {
    // Eigen::Map<Eigen::MatrixXd> q(mapStateEigen<0>(world).data(), V.cols(), V.rows()); // Get displacements only
    auto q = mapDOFEigen(tets->getQ(), world);
    Eigen::Map<Eigen::MatrixXd> dV(q.data(), V.cols(), V.rows()); // Get displacements only

    return V + dV.transpose(); 
}


void reset_world (MyWorld &world) {
        auto q = mapStateEigen(world); // TODO is this necessary?
        q.setZero();
}
using Eigen::VectorXd;
using Eigen::MatrixXd;
using namespace LBFGSpp;

class GPLC
{
private:
    VectorXd cur_q;
    VectorXd prev_q;
    VectorXd F_ext;
    MatrixXd M;
    double h;
public:
    GPLC(VectorXd cur_q, VectorXd prev_q, MatrixXd M, double h) : cur_q(cur_q), prev_q(prev_q), M(M), h(h) {
        VectorXd g(cur_q.size());
        for(int i=0; i < g.size(); i += 3) {
            g[i] = 0.0;
            g[i+1] = -9.8;
            g[i+2] = 0.0;
        }

        F_ext = M * g;

        std::cout << "g" << std::endl;
        std::cout << F_ext << std::endl;


    }
    
    double operator()(const VectorXd& new_q, VectorXd& grad)
    {
        double fx = 0.0;
        VectorXd A = new_q - 2.0 * cur_q + prev_q;
        
        double energy = tets->getEnergy(world.getState()); // TODO
        fx = M * A - h * 
        // double fx = 0.0;
        // for(int i = 0; i < n; i += 2)
        // {
        //     double t1 = 1.0 - x[i];
        //     double t2 = 10 * (x[i + 1] - x[i] * x[i]);
        //     grad[i + 1] = 20 * t2;
        //     grad[i]     = -2.0 * (x[i] * grad[i + 1] + t1);
        //     fx += t1 * t1 + t2 * t2;
        // }
        return fx;
    }
};

void test_opt(MyWorld &world, NeohookeanTets *tets) {
    const int n = 10;
    // Set up parameters
    LBFGSParam<double> param;
    param.epsilon = 1e-6;
    param.max_iterations = 100;

    // Create solver and function object
    LBFGSSolver<double> solver(param);

    auto q = mapDOFEigen(tets->getQ(), world);

    AssemblerEigenSparseMatrix<double> M;
    getMassMatrix(M, world);
    //AssemblerEigenVector<double> force

    GPLC fun(q, q, *M, 0.001);


    return;

    // Initial guess
    VectorXd x = VectorXd::Zero(n);
    // x will be overwritten to be the best point found
    double fx;
    int niter = solver.minimize(fun, x, fx);

    std::cout << niter << " iterations" << std::endl;
    std::cout << "x = \n" << x.transpose() << std::endl;
    std::cout << "f(x) = " << fx << std::endl;

    return;
}

// double objective()

int main(int argc, char **argv) {
    std::cout<<"Testt Neohookean FEM \n";
    
    //Setup Physics
    MyWorld world;
    std::cout<<"defined world\n";
    igl::readMESH(argv[1], V, T, F);
    NeohookeanTets *tets = new NeohookeanTets(V,T);

    world.addSystem(tets);
    fixDisplacementMin(world, tets);
    world.finalize(); //After this all we're ready to go (clean up the interface a bit later)
    
    reset_world(world);

    MyTimeStepper stepper(0.05);
    stepper.step(world);


    test_opt(world, tets);
    return 0;
    /** libigl display stuff **/
    igl::viewer::Viewer viewer;

    viewer.callback_pre_draw = [&](igl::viewer::Viewer & viewer)
    {
        // predefined colors
        const Eigen::RowVector3d orange(1.0,0.7,0.2);
        const Eigen::RowVector3d yellow(1.0,0.9,0.2);
        const Eigen::RowVector3d blue(0.2,0.3,0.8);
        const Eigen::RowVector3d green(0.2,0.6,0.3);



        if(viewer.core.is_animating)
        {   
            // Save Current configuration
            std::stringstream filename;
            filename << output_dir << "displacements_" << current_frame << ".dmat";
            if(saving_training_data) {
                save_displacements_DMAT(filename.str(), world, tets);
            }

            // stepper.step(world);
            auto q = mapDOFEigen(tets->getQ(), world);
            std::cout << tets->getEnergy(world.getState()) << std::endl;
            q *= 1.01;

            Eigen::MatrixXd newV = getCurrentVertPositions(world, tets); 
            // std::cout<< newV.block(0,0,10,3) << std::endl;
            viewer.data.set_vertices(newV);
            viewer.data.compute_normals();

            current_frame++;
        }
        return false;
    };

    viewer.callback_key_pressed = [&](igl::viewer::Viewer &, unsigned int key, int mod)
    {
        switch(key)
        {
            case 'P':
            case 'p':
            {
                viewer.core.is_animating = !viewer.core.is_animating;
                break;
            }
            case 'r':
            case 'R':
            {
                reset_world(world);
                break;
            }
            default:
            return false;
        }
        return true;
    };

    // viewer.callback_mouse_down = [&](igl::viewer::Viewer&, int, int)->bool
    // {   
    //     Eigen::MatrixXd curV = getCurrentVertPositions(world, tets); 
    //     last_mouse = Eigen::RowVector3f(viewer.current_mouse_x,viewer.core.viewport(3)-viewer.current_mouse_y,0);
    //     std::cout << last_mouse << std::endl;
    //     // Find closest point on mesh to mouse position
    //     int fid;
    //     Eigen::Vector3f bary;
    //     if(igl::unproject_onto_mesh(
    //         last_mouse.head(2),
    //         viewer.core.view * viewer.core.model,
    //         viewer.core.proj, 
    //         viewer.core.viewport, 
    //         curV, F, 
    //         fid, bary))
    //     {
    //         long c;
    //         bary.maxCoeff(&c);
    //         dragged_pos = curV.row(F(fid,c)) + Eigen::RowVector3d(0.001,0.0,0.0); //Epsilon offset so we don't div by 0
    //         dragged_vert = F(fid,c);
    //         std::cout << "Dragging vert: " << dragged_vert << std::endl;

    //         // Update the system
    //         is_dragging = true;
    //         forceSpring->getImpl().setStiffness(spring_stiffness);
    //         auto pinned_q = mapDOFEigen(pinned_point->getQ(), world);
    //         pinned_q = dragged_pos;//(dragged_pos).cast<double>(); // necessary?

    //         fem_attached_pos = PosFEM<double>(&tets->getQ()[dragged_vert],dragged_vert, &tets->getImpl().getV());
    //         forceSpring->getImpl().setPosition0(fem_attached_pos);

    //         return true;
    //     }
        
    //     return false; // TODO false vs true??
    // };

    // viewer.callback_mouse_up = [&](igl::viewer::Viewer&, int, int)->bool
    // {
    //     is_dragging = false;
    //     forceSpring->getImpl().setStiffness(0.0);
    //     // auto pinned_q = mapDOFEigen(pinned_point->getQ(), world);
    //     // pinned_q = Eigen::RowVector3d(-10000.0,0.0,0.0); // Move the point out of view

    //     return false;
    // };

    // viewer.callback_mouse_move = [&](igl::viewer::Viewer &, int,int)->bool
    // {
    //     if(is_dragging) {
    //         Eigen::RowVector3f drag_mouse(
    //             viewer.current_mouse_x,
    //             viewer.core.viewport(3) - viewer.current_mouse_y,
    //             last_mouse(2));

    //         Eigen::RowVector3f drag_scene,last_scene;

    //         igl::unproject(
    //             drag_mouse,
    //             viewer.core.view*viewer.core.model,
    //             viewer.core.proj,
    //             viewer.core.viewport,
    //             drag_scene);
    //         igl::unproject(
    //             last_mouse,
    //             viewer.core.view*viewer.core.model,
    //             viewer.core.proj,
    //             viewer.core.viewport,
    //             last_scene);

    //         dragged_pos += ((drag_scene-last_scene)*4.5).cast<double>(); //TODO why do I need to fine tune this
    //         // dragged_pos += (drag_mouse-last_mouse).cast<double>();
    //         last_mouse = drag_mouse;

    //         // Update the system
    //         // TODO dedupe this
    //         auto pinned_q = mapDOFEigen(pinned_point->getQ(), world);
    //         pinned_q = dragged_pos; //(dragged_pos).cast<double>(); // necessary?
    //     }

    //     return false;
    // };

    viewer.data.set_mesh(V,F);
    viewer.core.show_lines = true;
    viewer.core.invert_normals = true;
    viewer.core.is_animating = false;
    viewer.data.face_based = true;

    viewer.launch();
    return EXIT_SUCCESS;
}

