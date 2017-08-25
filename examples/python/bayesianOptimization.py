#!/usr/bin/env python
#  <!--------------------------------------------------------------------------
#  This file is part of libSBMLSim.  Please visit
#  http://fun.bio.keio.ac.jp/software/libsbmlsim/ for more
#  information about libSBMLSim and its latest version.
# 
#  Copyright (C) 2011-2017 by the Keio University, Yokohama, Japan
# 
#  This library is free software; you can redistribute it and/or modify it
#  under the terms of the GNU Lesser General Public License as published by
#  the Free Software Foundation.  A copy of the license agreement is provided
#  in the file named "LICENSE.txt" included with this software distribution.
#  ---------------------------------------------------------------------- -->

#
# This script will run a grid search for a given global parameter and
# calculate error from a given experimental data.
#
from libsbml import *
from libsbmlsim import *
from numpy import genfromtxt
from numpy import linspace
from sigopt import Connection

api_token = "YOUR_API_TOKEN"
development_token = "YOUR_DEVELOPMENT_TOKEN"
my_token = api_token

def main():
    # Experimental Data
    datafile = './data.csv'
    data = importData(datafile)
    # SBML model
    modelfile = './simple.xml'
    d = readSBML(modelfile)
    # Simulation
    simulation_time = 10
    dt = 0.1
    # Bayesian Optimization with SigOpt
    param_id = 'k'
    min_value = 0
    max_value = 1.0
    grid_spacing = 0.05
    grid_points = (max_value - min_value) / grid_spacing + 1
    ## Set up the experiment
    conn = Connection(client_token=my_token)
    experiment = conn.experiments().create(
        name=d.getModel().getId() + " SBML model",
        parameters=[
            dict(name=param_id, type='double', bounds=dict(min=min_value, max=max_value)),
        ],
    )
    print("Created experiment: https://sigopt.com/experiment/" + experiment.id)
    # Run the Optimization Loop between 10x - 20x the number of parameters
    for _ in range(20):
        suggestion = conn.experiments(experiment.id).suggestions().create()
        print _ , "suggestion:" , param_id, "=", suggestion.assignments[param_id]
        updateParameter(d, param_id, suggestion.assignments[param_id])
        result = integrate(d, simulation_time, dt)
        value = calcError(result, data) * -1
        print "  value =", value
        #value = evaluate_model(suggestion.assignments)
        conn.experiments(experiment.id).observations().create(
            suggestion=suggestion.id,
            value=value,
        )

    best_assignments_list = (
        conn.experiments(experiment.id)
            .best_assignments()
            .fetch()
    )
    if best_assignments_list.data:
        #print best_assignments_list.data
        best_assignments = best_assignments_list.data[0].assignments
        print best_assignments

def updateParameter(sbml, parameter_id, new_value):
    m = sbml.getModel()
    p = m.getParameter(parameter_id)
    p.setValue(new_value)

def calcError(result, data):
    error = 0.0
    numOfRows = result.getNumOfRows()
    numOfSp = result.getNumOfSpecies()
    for i in range(numOfRows):
        t = result.getTimeValueAtIndex(i)
        for di in data:
            if di['time'] == t:
                for j in range(numOfSp):
                    sname = result.getSpeciesNameAtIndex(j)
                    value_sim = result.getSpeciesValueAtIndex(sname, i)
                    error += (value_sim - di[sname])**2

    return error

def integrate(sbml, simulation_time, dt):
    result = simulateSBMLFromString(sbml.toSBML(), simulation_time, dt, 1, 0, MTHD_RUNGE_KUTTA, 0)
    if result.isError():
        print result.getErrorMessage()

    return result

def debugPrint(result):
    numOfRows = result.getNumOfRows()
    print "numOfRows: " + str(numOfRows)

    numOfSp = result.getNumOfSpecies()
    print "numOfSpecies: " + str(numOfSp)

    numOfParam = result.getNumOfParameters()
    print "numOfParameters: " + str(numOfParam)

    numOfComp = result.getNumOfCompartments()
    print "numOfCompartments: " + str(numOfComp)

    timeName = result.getTimeName()
    print "timeName: " + timeName

    print "Species Name"
    for i in range(numOfSp):
        sname = result.getSpeciesNameAtIndex(i)
        print "  " + sname

    print "Parameters Name"
    for i in range(numOfParam):
        pname = result.getParameterNameAtIndex(i)
        print "  " + pname

    print "Compartment Name"
    for i in range(numOfComp):
        cname = result.getCompartmentNameAtIndex(i)
        print "  " + cname

    print "Values"
    for i in range(numOfRows):
        t = result.getTimeValueAtIndex(i)
        print t,
        for j in range(numOfSp):
            sname = result.getSpeciesNameAtIndex(j)
            v = result.getSpeciesValueAtIndex(sname, i)
            print str(v),
        print

def importData(datafile):
    data = genfromtxt(datafile, delimiter=',', names=True)
    print "Experimental Data"
    print data.dtype.names
    print data
    return data

if __name__ == "__main__":
  main()

