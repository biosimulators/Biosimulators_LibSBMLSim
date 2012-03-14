#include <time.h>
#include "libsbmlsim/libsbmlsim.h"

//coefficient matrix for Adams Bashforth
double c_e[4][4] = {{1.0, 0, 0, 0}, //AB1 (Euler) : order = 0
  {3.0/2.0, -1.0/2.0, 0, 0}, //AB2 : order = 1
  {23.0/12.0, -16.0/12.0, 5.0/12.0, 0}, //AB3 : order = 2
  {55.0/24.0, -59.0/24.0, 37.0/24.0, -9.0/24.0}}; //AB4 : order = 3

double calc_explicit_formula(int order, double k1, double k2, double k3, double k4){
  return c_e[order][0]*k1 + c_e[order][1]*k2 + c_e[order][2]*k3 + c_e[order][3]*k4;
}

void seed_set_ex(){
  srand((unsigned)time(NULL));
}

myResult* simulate_explicit(Model_t *m, myResult* result, mySpecies *sp[], myParameter *param[], myCompartment *comp[], myReaction *re[], myRule *rule[], myEvent *event[], myInitialAssignment *initAssign[], myAlgebraicEquations *algEq, timeVariantAssignments *timeVarAssign, double sim_time, double dt, int print_interval, double *time, int order, int print_amount, allocated_memory *mem){
  int i, j, cycle;
  double reverse_time;
  double *value_time_p = result->values_time;
  double *value_sp_p = result->values_sp;
  double *value_param_p = result->values_param;
  double *value_comp_p = result->values_comp;
  int error;
  int end_cycle = get_end_cycle(sim_time, dt);
  int num_of_species = Model_getNumSpecies(m);
  int num_of_parameters = Model_getNumParameters(m);
  int num_of_compartments = Model_getNumCompartments(m);
  int num_of_reactions = Model_getNumReactions(m);
  int num_of_rules = Model_getNumRules(m);
  int num_of_events = Model_getNumEvents(m);
  int num_of_initialAssignments = Model_getNumInitialAssignments(m);

  int num_of_all_var_species = 0; //num of species whose quantity is not constant
  int num_of_all_var_parameters = 0; //num of parameters whose value is not constant
  int num_of_all_var_compartments = 0; //num of compartment whose value is not constant
  int num_of_all_var_species_reference = 0;
  int num_of_var_species = 0; //num of species whose quantity changes, but not by assignment, algebraic rule
  int num_of_var_parameters = 0; //num of parameters whose value changes, but not by assignment, algebraic rule
  int num_of_var_compartments = 0; //num of compartments whose value changes, but not by assignment, algebraic rule
  int num_of_var_species_reference = 0;

  seed_set_ex();

  check_num(num_of_species, num_of_parameters, num_of_compartments, num_of_reactions, &num_of_all_var_species, &num_of_all_var_parameters, &num_of_all_var_compartments, &num_of_all_var_species_reference, &num_of_var_species, &num_of_var_parameters, &num_of_var_compartments, &num_of_var_species_reference, sp, param, comp, re);

  mySpecies *all_var_sp[num_of_all_var_species]; //all variable species
  myParameter *all_var_param[num_of_all_var_parameters]; //all variable parameters
  myCompartment *all_var_comp[num_of_all_var_compartments]; //all variable compartments
  mySpeciesReference *all_var_spr[num_of_all_var_species_reference];
  mySpecies *var_sp[num_of_var_species]; //variable species (species which change their value with assignment and algebraic rule are excluded)
  myParameter *var_param[num_of_var_parameters]; //variable parameters (parameters which change their value with assignment and algebraic rule are excluded)
  myCompartment *var_comp[num_of_var_compartments]; //variable compartments (parameters which change their value with assignment and algebraic rule are excluded)
  mySpeciesReference *var_spr[num_of_var_species_reference];

  create_calc_object_list(num_of_species, num_of_parameters, num_of_compartments, num_of_reactions, num_of_all_var_species, num_of_all_var_parameters, num_of_all_var_compartments, num_of_all_var_species_reference, num_of_var_species, num_of_var_parameters, num_of_var_compartments, num_of_var_species_reference, all_var_sp, all_var_param, all_var_comp, all_var_spr, var_sp, var_param, var_comp, var_spr, sp, param, comp, re);

  double **coefficient_matrix = NULL;
  double *constant_vector = NULL;
  int *alg_pivot = NULL;
  if(algEq != NULL){
    coefficient_matrix = (double**)malloc(sizeof(double*)*(algEq->num_of_algebraic_variables));
    for(i=0; i<algEq->num_of_algebraic_variables; i++){
      coefficient_matrix[i] = (double*)malloc(sizeof(double)*(algEq->num_of_algebraic_variables));
    }
    constant_vector = (double*)malloc(sizeof(double)*(algEq->num_of_algebraic_variables));
    alg_pivot = (int*)malloc(sizeof(int)*(algEq->num_of_algebraic_variables));
  }

  double reactants_numerator, products_numerator;
  double min_value;

  prg_printf("Simulation for [%s] Starts!\n", Model_getId(m));
  cycle = 0;

  //initialize delay_val
  initialize_delay_val(sp, num_of_species, param, num_of_parameters, comp, num_of_compartments, re, num_of_reactions, sim_time, dt, 0);

  //calc temp value by assignment
  for(i=0; i<num_of_all_var_species; i++){
    if(all_var_sp[i]->depending_rule != NULL && all_var_sp[i]->depending_rule->is_assignment){
      all_var_sp[i]->temp_value = calc(all_var_sp[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
    }
  }
  for(i=0; i<num_of_all_var_parameters; i++){
    if(all_var_param[i]->depending_rule != NULL && all_var_param[i]->depending_rule->is_assignment){
      all_var_param[i]->temp_value = calc(all_var_param[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
    }
  }
  for(i=0; i<num_of_all_var_compartments; i++){
    if(all_var_comp[i]->depending_rule != NULL && all_var_comp[i]->depending_rule->is_assignment){
      all_var_comp[i]->temp_value = calc(all_var_comp[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
    }
  }
  for(i=0; i<num_of_all_var_species_reference; i++){
    if(all_var_spr[i]->depending_rule != NULL && all_var_spr[i]->depending_rule->is_assignment){
      all_var_spr[i]->temp_value = calc(all_var_spr[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
    }
  }
  //forwarding value
  forwarding_value(all_var_sp, num_of_all_var_species, all_var_param, num_of_all_var_parameters, all_var_comp, num_of_all_var_compartments, all_var_spr, num_of_all_var_species_reference);

  //initialize delay_val
  initialize_delay_val(sp, num_of_species, param, num_of_parameters, comp, num_of_compartments, re, num_of_reactions, sim_time, dt, 0);

  //calc InitialAssignment
  calc_initial_assignment(initAssign, num_of_initialAssignments, dt, cycle, &reverse_time);

  //initialize delay_val
  initialize_delay_val(sp, num_of_species, param, num_of_parameters, comp, num_of_compartments, re, num_of_reactions, sim_time, dt, 0);

  //rewriting for explicit delay
  double *init_val;
  for(i=0; i<num_of_initialAssignments; i++){
    for(j=0; j<initAssign[i]->eq->math_length; j++){
      if(initAssign[i]->eq->number[j] == time){
        dbg_printf("time is replaced with reverse time\n");
        initAssign[i]->eq->number[j] = &reverse_time;
      }else if(initAssign[i]->eq->number[j] != NULL){
        init_val = (double*)malloc(sizeof(double));
        *init_val = *initAssign[i]->eq->number[j];
        mem->memory[mem->num_of_allocated_memory++] = init_val;
        initAssign[i]->eq->number[j] = init_val;
      }
    }
  }
  for(i=0; i<timeVarAssign->num_of_time_variant_assignments; i++){
    for(j=0; j<timeVarAssign->eq[i]->math_length; j++){
      if(timeVarAssign->eq[i]->number[j] == time){
        dbg_printf("time is replaced with reverse time\n");
        timeVarAssign->eq[i]->number[j] = &reverse_time;
      }else if(timeVarAssign->eq[i]->number[j] != NULL){
        init_val = (double*)malloc(sizeof(double));
        *init_val = *timeVarAssign->eq[i]->number[j];
        mem->memory[mem->num_of_allocated_memory++] = init_val;
        timeVarAssign->eq[i]->number[j] = init_val;
      }
    }
  }

  //initialize delay_val
  initialize_delay_val(sp, num_of_species, param, num_of_parameters, comp, num_of_compartments, re, num_of_reactions, sim_time, dt, 0);

  //calc temp value by assignment
  for(i=0; i<num_of_all_var_species; i++){
    if(all_var_sp[i]->depending_rule != NULL && all_var_sp[i]->depending_rule->is_assignment){
      all_var_sp[i]->temp_value = calc(all_var_sp[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
    }
  }
  for(i=0; i<num_of_all_var_parameters; i++){
    if(all_var_param[i]->depending_rule != NULL && all_var_param[i]->depending_rule->is_assignment){
      all_var_param[i]->temp_value = calc(all_var_param[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
    }
  }
  for(i=0; i<num_of_all_var_compartments; i++){
    if(all_var_comp[i]->depending_rule != NULL && all_var_comp[i]->depending_rule->is_assignment){
      all_var_comp[i]->temp_value = calc(all_var_comp[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
    }
  }
  for(i=0; i<num_of_all_var_species_reference; i++){
    if(all_var_spr[i]->depending_rule != NULL && all_var_spr[i]->depending_rule->is_assignment){
      all_var_spr[i]->temp_value = calc(all_var_spr[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
    }
  }
  //forwarding value
  forwarding_value(all_var_sp, num_of_all_var_species, all_var_param, num_of_all_var_parameters, all_var_comp, num_of_all_var_compartments, all_var_spr, num_of_all_var_species_reference);

  //initialize delay_val
  initialize_delay_val(sp, num_of_species, param, num_of_parameters, comp, num_of_compartments, re, num_of_reactions, sim_time, dt, 0);

  //calc temp value algebraic by algebraic
  if(algEq != NULL){
    if(algEq->num_of_algebraic_variables > 1){
      //initialize pivot
      for(i=0; i<algEq->num_of_algebraic_variables; i++){
        alg_pivot[i] = i;
      }
      for(i=0; i<algEq->num_of_algebraic_variables; i++){
        for(j=0; j<algEq->num_of_algebraic_variables; j++){
          coefficient_matrix[i][j] = calc(algEq->coefficient_matrix[i][j], dt, cycle, &reverse_time, 0);
          //dbg_printf("coefficient matrix[%d][%d] = %lf\n", i, j, coefficient_matrix[i][j]);
        }
      }
      for(i=0; i<algEq->num_of_algebraic_variables; i++){
        constant_vector[i] = -calc(algEq->constant_vector[i], dt, cycle, &reverse_time, 0);
        //dbg_printf("constant vector[%d] = %lf\n", i, constant_vector[i]);
      }
      //LU decompostion
      error = lu_decomposition(coefficient_matrix, alg_pivot, algEq->num_of_algebraic_variables);
      if(error == 0){//failure in LU decomposition
        return NULL;
      }
      //forward substitution & backward substitution
      lu_solve(coefficient_matrix, alg_pivot, algEq->num_of_algebraic_variables, constant_vector);
      /*       for(i=0; i<algEq->num_of_algebraic_variables; i++){ */
      /* 	dbg_printf("ans[%d] = %lf\n", i, constant_vector[i]); */
      /*       } */
      for(i=0; i<algEq->num_of_alg_target_sp; i++){
        algEq->alg_target_species[i]->target_species->temp_value = constant_vector[algEq->alg_target_species[i]->order];
      }
      for(i=0; i<algEq->num_of_alg_target_param; i++){
        algEq->alg_target_parameter[i]->target_parameter->temp_value = constant_vector[algEq->alg_target_parameter[i]->order];
      }    
      for(i=0; i<algEq->num_of_alg_target_comp; i++){
        //new code
        for(j=0; j<algEq->alg_target_compartment[i]->target_compartment->num_of_including_species; j++){
          if(algEq->alg_target_compartment[i]->target_compartment->including_species[j]->is_concentration){
            algEq->alg_target_compartment[i]->target_compartment->including_species[j]->temp_value = algEq->alg_target_compartment[i]->target_compartment->including_species[j]->temp_value*algEq->alg_target_compartment[i]->target_compartment->temp_value/constant_vector[algEq->alg_target_compartment[i]->order];
          }
        }
        //
        algEq->alg_target_compartment[i]->target_compartment->temp_value = constant_vector[algEq->alg_target_compartment[i]->order];
      }    
    }else{
      if(algEq->target_species != NULL){
        algEq->target_species->temp_value = -calc(algEq->constant, dt, cycle, &reverse_time, 0)/calc(algEq->coefficient, dt, cycle, &reverse_time, 0);
      }
      if(algEq->target_parameter != NULL){
        algEq->target_parameter->temp_value = -calc(algEq->constant, dt, cycle, &reverse_time, 0)/calc(algEq->coefficient, dt, cycle, &reverse_time, 0);
      }
      if(algEq->target_compartment != NULL){
        //new code
        for(i=0; i<algEq->target_compartment->num_of_including_species; i++){
          if(algEq->target_compartment->including_species[i]->is_concentration){
            algEq->target_compartment->including_species[i]->temp_value = algEq->target_compartment->including_species[i]->temp_value*algEq->target_compartment->temp_value/(-calc(algEq->constant, dt, cycle, &reverse_time, 0)/calc(algEq->coefficient, dt, cycle, &reverse_time, 0));
          }
        }
        //
        algEq->target_compartment->temp_value = -calc(algEq->constant, dt, cycle, &reverse_time, 0)/calc(algEq->coefficient, dt, cycle, &reverse_time, 0);
      }
    }
    //forwarding value
    forwarding_value(all_var_sp, num_of_all_var_species, all_var_param, num_of_all_var_parameters, all_var_comp, num_of_all_var_compartments, all_var_spr, num_of_all_var_species_reference);
  }

  //initialize delay_val
  initialize_delay_val(sp, num_of_species, param, num_of_parameters, comp, num_of_compartments, re, num_of_reactions, sim_time, dt, 1);

  //cycle start
  for(cycle=0; cycle<=end_cycle; cycle++){
    //calculate unreversible fast reaction
    for(i=0; i<num_of_reactions; i++){
      if(re[i]->is_fast && !re[i]->is_reversible){
        if(calc(re[i]->eq, dt, cycle, &reverse_time, 0) > 0){
          min_value = DBL_MAX;
          for(j=0; j<re[i]->num_of_reactants; j++){
            if(min_value > re[i]->reactants[j]->mySp->value/calc(re[i]->reactants[j]->eq, dt, cycle, &reverse_time, 0)){
              min_value = re[i]->reactants[j]->mySp->value/calc(re[i]->reactants[j]->eq, dt, cycle, &reverse_time, 0);
            }
          }
          for(j=0; j<re[i]->num_of_products; j++){
            if(!Species_getBoundaryCondition(re[i]->products[j]->mySp->origin)){
              re[i]->products[j]->mySp->value += calc(re[i]->products[j]->eq, dt, cycle, &reverse_time, 0)*min_value;
              re[i]->products[j]->mySp->temp_value = re[i]->products[j]->mySp->value;
            }
          }
          for(j=0; j<re[i]->num_of_reactants; j++){
            if(!Species_getBoundaryCondition(re[i]->reactants[j]->mySp->origin)){
              re[i]->reactants[j]->mySp->value -= calc(re[i]->reactants[j]->eq, dt, cycle, &reverse_time, 0)*min_value;
              re[i]->reactants[j]->mySp->temp_value = re[i]->reactants[j]->mySp->value;
            }
          }
        }
      }
    }
    //calculate reversible fast reactioin
    for(i=0; i<num_of_reactions; i++){
      if(re[i]->is_fast && re[i]->is_reversible){
        if(!(Species_getBoundaryCondition(re[i]->products[0]->mySp->origin) 
              && Species_getBoundaryCondition(re[i]->reactants[0]->mySp->origin))){
          products_numerator = calc(re[i]->products_equili_numerator, dt, cycle, &reverse_time, 0);
          reactants_numerator = calc(re[i]->reactants_equili_numerator, dt, cycle, &reverse_time, 0);
          if(products_numerator > 0 || reactants_numerator > 0){
            if(Species_getBoundaryCondition(re[i]->products[0]->mySp->origin)){
              re[i]->reactants[0]->mySp->value = (reactants_numerator/products_numerator)*re[i]->products[0]->mySp->value;
              re[i]->reactants[0]->mySp->temp_value = re[i]->reactants[0]->mySp->value;
            }else if(Species_getBoundaryCondition(re[i]->reactants[0]->mySp->origin)){
              re[i]->products[0]->mySp->value = (products_numerator/reactants_numerator)*re[i]->reactants[0]->mySp->value;
              re[i]->products[0]->mySp->temp_value = re[i]->products[0]->mySp->value;	    
            }else{
              re[i]->products[0]->mySp->value = (products_numerator/(products_numerator+reactants_numerator))*(re[i]->products[0]->mySp->temp_value+re[i]->reactants[0]->mySp->temp_value);
              re[i]->reactants[0]->mySp->value = (reactants_numerator/(products_numerator+reactants_numerator))*(re[i]->products[0]->mySp->temp_value+re[i]->reactants[0]->mySp->temp_value);
              re[i]->products[0]->mySp->temp_value = re[i]->products[0]->mySp->value;
              re[i]->reactants[0]->mySp->temp_value = re[i]->reactants[0]->mySp->value;
            }
          }
        }
      }
    }

    //event
    calc_event(event, num_of_events, dt, *time, cycle, &reverse_time);

    //substitute delay val
    substitute_delay_val(sp, num_of_species, param, num_of_parameters, comp, num_of_compartments, re, num_of_reactions, cycle);

    //progress
    if(cycle%(int)(end_cycle/10) == 0){
      prg_printf("%3d %%\n", (int)(100*((double)cycle/(double)end_cycle)));
      prg_printf("\x1b[1A");
      prg_printf("\x1b[5D");
    }
    //print result
    if(cycle%print_interval == 0){
      // Time
      *value_time_p = *time;
      value_time_p++;
      // Species
      for(i=0; i<num_of_species; i++){
        //        if(!(Species_getConstant(sp[i]->origin) && Species_getBoundaryCondition(sp[i]->origin))){ // XXX must remove this
        if(print_amount){
          if(sp[i]->is_concentration){
            *value_sp_p = sp[i]->value*sp[i]->locating_compartment->value;
          }else{
            *value_sp_p = sp[i]->value;
          }
        }else{
          if(sp[i]->is_amount){
            *value_sp_p = sp[i]->value/sp[i]->locating_compartment->value;
          }else{
            *value_sp_p = sp[i]->value;
          }
        }
        value_sp_p++;
        //        }
      }
      // Parameter
      for(i=0; i<num_of_parameters; i++){
        //        if(!Parameter_getConstant(param[i]->origin)){ // XXX must remove this
        *value_param_p = param[i]->value;
        //        }
        value_param_p++;
      }
      // Compartment
      for(i=0; i<num_of_compartments; i++){
        //        if(!Compartment_getConstant(comp[i]->origin)){ // XXX must remove this
        *value_comp_p = comp[i]->value;
        //        }
        value_comp_p++;
      }
    }

    //time increase
    *time = (cycle+1)*dt;

    if(order == 4){//runge kutta      
      calc_k(all_var_sp, num_of_all_var_species, all_var_param, num_of_all_var_parameters, all_var_comp, num_of_all_var_compartments, all_var_spr, num_of_all_var_species_reference, re, num_of_reactions, rule, num_of_rules, cycle, dt, &reverse_time, 1, 1);
      calc_temp_value(all_var_sp, num_of_all_var_species, all_var_param, num_of_all_var_parameters, all_var_comp, num_of_all_var_compartments, all_var_spr, num_of_all_var_species_reference, cycle, dt, 1);      
    }else{//Adams-Bashforth
      //calc k
      calc_k(var_sp, num_of_var_species, var_param, num_of_var_parameters, var_comp, num_of_var_compartments, var_spr, num_of_var_species_reference, re, num_of_reactions, rule, num_of_rules, cycle, dt, &reverse_time, 0, 1);      
      //calc temp value by Adams Bashforth
      for(i=0; i<num_of_var_species; i++){
        var_sp[i]->temp_value = var_sp[i]->value + calc_explicit_formula(order, var_sp[i]->k[0], var_sp[i]->prev_k[0], var_sp[i]->prev_k[1], var_sp[i]->prev_k[2])*dt;
      }
      for(i=0; i<num_of_var_parameters; i++){
        var_param[i]->temp_value = var_param[i]->value + calc_explicit_formula(order, var_param[i]->k[0], var_param[i]->prev_k[0], var_param[i]->prev_k[1], var_param[i]->prev_k[2])*dt;
      }
      for(i=0; i<num_of_var_compartments; i++){
        var_comp[i]->temp_value = var_comp[i]->value + calc_explicit_formula(order, var_comp[i]->k[0], var_comp[i]->prev_k[0], var_comp[i]->prev_k[1], var_comp[i]->prev_k[2])*dt;
      }
      for(i=0; i<num_of_var_species_reference; i++){
        var_spr[i]->temp_value = var_spr[i]->value + calc_explicit_formula(order, var_spr[i]->k[0], var_spr[i]->prev_k[0], var_spr[i]->prev_k[1], var_spr[i]->prev_k[2])*dt;
      }      
      //calc temp value by assignment
      for(i=0; i<num_of_all_var_species; i++){
        if(all_var_sp[i]->depending_rule != NULL && all_var_sp[i]->depending_rule->is_assignment){
          all_var_sp[i]->temp_value = calc(all_var_sp[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
        }
      }
      for(i=0; i<num_of_all_var_parameters; i++){
        if(all_var_param[i]->depending_rule != NULL && all_var_param[i]->depending_rule->is_assignment){
          all_var_param[i]->temp_value = calc(all_var_param[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
        }
      }
      for(i=0; i<num_of_all_var_compartments; i++){
        if(all_var_comp[i]->depending_rule != NULL && all_var_comp[i]->depending_rule->is_assignment){
          all_var_comp[i]->temp_value = calc(all_var_comp[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
        }
      }
      for(i=0; i<num_of_all_var_species_reference; i++){
        if(all_var_spr[i]->depending_rule != NULL && all_var_spr[i]->depending_rule->is_assignment){
          all_var_spr[i]->temp_value = calc(all_var_spr[i]->depending_rule->eq, dt, cycle, &reverse_time, 0);
        }
      }
    }

    //calc temp value algebraic by algebraic
    if(algEq != NULL){
      if(algEq->num_of_algebraic_variables > 1){
        //initialize pivot
        for(i=0; i<algEq->num_of_algebraic_variables; i++){
          alg_pivot[i] = i;
        }
        for(i=0; i<algEq->num_of_algebraic_variables; i++){
          for(j=0; j<algEq->num_of_algebraic_variables; j++){
            coefficient_matrix[i][j] = calc(algEq->coefficient_matrix[i][j], dt, cycle, &reverse_time, 0);
          }
        }
        for(i=0; i<algEq->num_of_algebraic_variables; i++){
          constant_vector[i] = -calc(algEq->constant_vector[i], dt, cycle, &reverse_time, 0);
        }
        //LU decompostion
        error = lu_decomposition(coefficient_matrix, alg_pivot, algEq->num_of_algebraic_variables);
        if(error == 0){//failure in LU decomposition
          return NULL;
        }
        //forward substitution & backward substitution
        lu_solve(coefficient_matrix, alg_pivot, algEq->num_of_algebraic_variables, constant_vector);
        for(i=0; i<algEq->num_of_alg_target_sp; i++){
          algEq->alg_target_species[i]->target_species->temp_value = constant_vector[algEq->alg_target_species[i]->order];
        }    
        for(i=0; i<algEq->num_of_alg_target_param; i++){
          algEq->alg_target_parameter[i]->target_parameter->temp_value = constant_vector[algEq->alg_target_parameter[i]->order];
        }    
        for(i=0; i<algEq->num_of_alg_target_comp; i++){
          //new code
          for(j=0; j<algEq->alg_target_compartment[i]->target_compartment->num_of_including_species; j++){
            if(algEq->alg_target_compartment[i]->target_compartment->including_species[j]->is_concentration){
              algEq->alg_target_compartment[i]->target_compartment->including_species[j]->temp_value = algEq->alg_target_compartment[i]->target_compartment->including_species[j]->temp_value*algEq->alg_target_compartment[i]->target_compartment->temp_value/constant_vector[algEq->alg_target_compartment[i]->order];
            }
          }
          //
          algEq->alg_target_compartment[i]->target_compartment->temp_value = constant_vector[algEq->alg_target_compartment[i]->order];
        }    
      }else{
        if(algEq->target_species != NULL){
          algEq->target_species->temp_value = -calc(algEq->constant, dt, cycle, &reverse_time, 0)/calc(algEq->coefficient, dt, cycle, &reverse_time, 0);
        }
        if(algEq->target_parameter != NULL){
          algEq->target_parameter->temp_value = -calc(algEq->constant, dt, cycle, &reverse_time, 0)/calc(algEq->coefficient, dt, cycle, &reverse_time, 0);
        }
        if(algEq->target_compartment != NULL){
          //new code
          for(i=0; i<algEq->target_compartment->num_of_including_species; i++){
            if(algEq->target_compartment->including_species[i]->is_concentration){
              algEq->target_compartment->including_species[i]->temp_value = algEq->target_compartment->including_species[i]->temp_value*algEq->target_compartment->temp_value/(-calc(algEq->constant, dt, cycle, &reverse_time, 0)/calc(algEq->coefficient, dt, cycle, &reverse_time, 0));
            }
          }
          //
          algEq->target_compartment->temp_value = -calc(algEq->constant, dt, cycle, &reverse_time, 0)/calc(algEq->coefficient, dt, cycle, &reverse_time, 0);
        }
      }
    }

    //preserve prev_value and prev_k
    for(i=0; i<num_of_var_species; i++){
      var_sp[i]->prev_val[2] = var_sp[i]->prev_val[1];
      var_sp[i]->prev_val[1] = var_sp[i]->prev_val[0];
      var_sp[i]->prev_val[0] = var_sp[i]->value;
      var_sp[i]->prev_k[2] = var_sp[i]->prev_k[1];
      var_sp[i]->prev_k[1] = var_sp[i]->prev_k[0];
      var_sp[i]->prev_k[0] = var_sp[i]->k[0];
    }
    for(i=0; i<num_of_var_parameters; i++){
      var_param[i]->prev_val[2] = var_param[i]->prev_val[1];
      var_param[i]->prev_val[1] = var_param[i]->prev_val[0];
      var_param[i]->prev_val[0] = var_param[i]->value;
      var_param[i]->prev_k[2] = var_param[i]->prev_k[1];
      var_param[i]->prev_k[1] = var_param[i]->prev_k[0];
      var_param[i]->prev_k[0] = var_param[i]->k[0];
    }
    for(i=0; i<num_of_var_compartments; i++){
      var_comp[i]->prev_val[2] = var_comp[i]->prev_val[1];
      var_comp[i]->prev_val[1] = var_comp[i]->prev_val[0];
      var_comp[i]->prev_val[0] = var_comp[i]->value;
      var_comp[i]->prev_k[2] = var_comp[i]->prev_k[1];
      var_comp[i]->prev_k[1] = var_comp[i]->prev_k[0];
      var_comp[i]->prev_k[0] = var_comp[i]->k[0];
    }
    for(i=0; i<num_of_var_species_reference; i++){
      var_spr[i]->prev_val[2] = var_spr[i]->prev_val[1];
      var_spr[i]->prev_val[1] = var_spr[i]->prev_val[0];
      var_spr[i]->prev_val[0] = var_spr[i]->value;
      var_spr[i]->prev_k[2] = var_spr[i]->prev_k[1];
      var_spr[i]->prev_k[1] = var_spr[i]->prev_k[0];
      var_spr[i]->prev_k[0] = var_spr[i]->k[0];
    }

    //forwarding value
    forwarding_value(all_var_sp, num_of_all_var_species, all_var_param, num_of_all_var_parameters, all_var_comp, num_of_all_var_compartments, all_var_spr, num_of_all_var_species_reference);
  }
  prg_printf("Simulation for [%s] Ends!\n", Model_getId(m));
  if(algEq != NULL){
    for(i=0; i<algEq->num_of_algebraic_variables; i++){
      free(coefficient_matrix[i]);
    }
    free(coefficient_matrix);
    free(constant_vector);
    free(alg_pivot);
  }
  return result;
}
