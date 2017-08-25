#include <OpenSim/OpenSim.h>
#include <OpenSim/Tools/InverseDynamicsTool.h>
#include <INDYGO.h>

using namespace OpenSim;

void testIsometricMuscleRoundtrip() {
    // Generate motion.
    Model model;
    model.setName("isometric_muscle");
    TimeSeriesTable states;
    {
        model.set_gravity(SimTK::Vec3(9.81, 0, 0));
        auto* body = new Body("body", 0.5, SimTK::Vec3(0), SimTK::Inertia(0));
        model.addComponent(body);

        // Allows translation along x.
        auto* joint = new SliderJoint("joint", model.getGround(), *body);
        auto& coord = joint->updCoordinate(SliderJoint::Coord::TranslationX);
        coord.setName("height");
        model.addComponent(joint);

        auto* actu = new Millard2012EquilibriumMuscle();
        actu->setName("actuator");
        actu->set_max_isometric_force(9.81);
        actu->set_optimal_fiber_length(0.10);
        actu->set_tendon_slack_length(0.10);
        actu->set_pennation_angle_at_optimal(0.1);
        actu->addNewPathPoint("origin", model.updGround(), SimTK::Vec3(0));
        actu->addNewPathPoint("insertion", *body, SimTK::Vec3(0));
        model.addComponent(actu);
        // TODO make this a muscle, and set its muscle parameters.

        auto* contr = new PrescribedController();
        contr->setName("controller");
        contr->addActuator(*actu);
        contr->prescribeControlForActuator("actuator", new Constant(0.5));
        model.addComponent(contr);

        auto* rep = new ConsoleReporter();
        rep->setName("reporter");
        rep->set_report_time_interval(0.1);
        rep->addToReport(coord.getOutput("value"), "height");
        rep->addToReport(actu->getOutput("actuation"), "applied_force");
        model.addComponent(rep);

        auto* statesRep = new StatesTrajectoryReporter();
        statesRep->setName("states_reporter");
        // This small interval is helpful for obtaining accurate estimates of
        // generalized accelerations, which are needed for inverse dynamics.
        statesRep->set_report_time_interval(0.001);
        model.addComponent(statesRep);

        // Simulate!
        SimTK::State state = model.initSystem();
        // optimal fiber length + tendon slack length.
        coord.setValue(state, 0.2);
        actu->setActivation(state, 0.5);
        actu->setFiberLength(state, 0.1);
        model.equilibrateMuscles(state);
        SimTK::RungeKuttaMersonIntegrator integrator(model.getSystem());
        // This is necessary to achieve a smooth solution for excitation.
        integrator.setAccuracy(1e-5);
        Manager manager(model, integrator);
        manager.integrate(state, 1.0);

        // Print the model and states trajectory to files.
        model.print("testSingleMuscle_isometric_muscle.osim");
        states = statesRep->getStates().exportToTable(model);
        STOFileAdapter_<double>::write(states,
            "testSingleMuscle_isometric_muscle_states.sto");
    }

    // Reconstruct actuation.
    {
        // Solve the problem.
        INDYGO mrs;
        mrs.setModel(model);
        mrs.setKinematicsData(states);
        mrs.set_lowpass_cutoff_frequency_for_joint_moments(6);
        // The static opt problem has "too few degrees of freedom."
        mrs.set_initial_guess("bounds");
        INDYGO::Solution solution = mrs.solve();
        solution.write("testSingleMuscle_isometric_muscle");

        // Check the answer. The differences in excitation and activation are
        // likely due to differences in the muscle model.
        auto compare = [](const TimeSeriesTable& table, double expected,
                          double tol) {
            const auto& actual = table.getDependentColumn(
                    "/isometric_muscle/actuator");
            SimTK::Vector expectedVector((int)table.getNumRows(), expected);
            // actual.dump("actual");
            SimTK_TEST_EQ_TOL(actual, expectedVector, tol);
        };

        compare(solution.excitation, 0.5, 0.01);
        compare(solution.activation, 0.5, 0.01);
        compare(solution.norm_fiber_velocity, 0.0, 0.01);
        // The fiber must be shorter than 0.1 meters so that the tendon is
        // not slack and can convey a force.
        compare(solution.norm_fiber_length, 0.98, 0.01);
    }
    // TODO test other muscle states (e.g, isometric at a greater
    // muscle-tendon length, and thus a different activation).
}

int main() {
    SimTK_START_TEST("testSingleMuscle");
        SimTK_SUBTEST(testIsometricMuscleRoundtrip);
    SimTK_END_TEST();
}

