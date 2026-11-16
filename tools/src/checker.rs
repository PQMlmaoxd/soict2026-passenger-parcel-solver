//! Independent simulation of a solution under the contest rules.
//!
//! The checker works on the raw vertex sequences the solver prints. It does not
//! know the solver's event model, so a modelling mistake in the solver cannot
//! hide in both places at once. A run only counts if this module accepts it.

use std::fs;
use std::path::Path;

use crate::instance::{invalid, parse_integer, read_instance, Failure, Instance, Kind, Result};

/// What a valid solution earns.
#[derive(Debug, PartialEq, Eq)]
pub struct Summary {
    pub benefit: i64,
    pub revenue: i64,
    pub travel_cost: i64,
    pub passengers: usize,
    pub parcels: usize,
}

fn add(left: i64, right: i64, field: &str) -> Result<i64> {
    match left.checked_add(right) {
        Some(value) => Ok(value),
        None => invalid(format!("{field} overflow")),
    }
}

/// Parses the solution file: the declared benefit, then one route per taxi.
pub fn read_solution(path: &Path, instance: &Instance) -> Result<(i64, Vec<Vec<usize>>)> {
    let bytes = fs::read(path)?;
    if !bytes.is_ascii() {
        return Err(Failure::Unreadable("solution is not ASCII".into()));
    }
    let text = String::from_utf8(bytes).expect("ascii is valid utf-8");
    let text = text.trim();
    if text.is_empty() {
        return invalid("empty solution");
    }
    let lines: Vec<&str> = text.lines().collect();
    if lines.len() != instance.taxis + 1 {
        return invalid(format!(
            "expected exactly {} output lines",
            instance.taxis + 1
        ));
    }
    let first: Vec<&str> = lines[0].split_whitespace().collect();
    if first.len() != 1 {
        return invalid("invalid declared benefit");
    }
    let benefit = parse_integer(first[0].as_bytes(), "declared benefit")?;

    let mut routes = Vec::with_capacity(instance.taxis);
    for (taxi, line) in lines[1..].iter().enumerate() {
        let fields: Vec<&str> = line.split_whitespace().collect();
        let size = match fields.first() {
            Some(field) => parse_integer(field.as_bytes(), "route size")
                .or_else(|_| invalid(format!("invalid route size for taxi {taxi}")))?,
            None => return invalid(format!("invalid route size for taxi {taxi}")),
        };
        if size < 0 || fields.len() as i64 != size + 1 {
            return invalid(format!("route count mismatch for taxi {taxi}"));
        }
        let mut route = Vec::with_capacity(fields.len() - 1);
        for field in &fields[1..] {
            let node = parse_integer(field.as_bytes(), "route vertex")
                .or_else(|_| invalid(format!("invalid route vertex for taxi {taxi}")))?;
            if node < 1 || node as u64 > instance.vertices as u64 {
                return invalid(format!("route vertex out of range for taxi {taxi}"));
            }
            route.push(node as usize);
        }
        routes.push(route);
    }
    Ok((benefit, routes))
}

/// Simulates every route and returns the benefit, or the first rule it breaks.
pub fn evaluate(instance: &Instance, routes: &[Vec<usize>]) -> Result<Summary> {
    let mut owner = std::collections::HashMap::new();
    for (taxi, route) in routes.iter().enumerate() {
        for (position, &node) in route.iter().enumerate() {
            if !instance.events.contains_key(&node) {
                return invalid(format!("route contains non-service vertex {node}"));
            }
            if owner.insert(node, (taxi, position)).is_some() {
                return invalid(format!("duplicate service vertex {node}"));
            }
        }
    }

    for (i, passenger) in instance.passengers.iter().enumerate() {
        match (owner.get(&passenger.pickup), owner.get(&passenger.drop)) {
            (None, None) => {}
            (Some(&(taxi_a, at)), Some(&(taxi_b, next))) => {
                if taxi_a != taxi_b || next != at + 1 {
                    return invalid(format!("passenger {i} drop is not directly after pickup"));
                }
            }
            _ => return invalid(format!("passenger {i} is partially served")),
        }
    }

    for (j, parcel) in instance.parcels.iter().enumerate() {
        match (owner.get(&parcel.pickup), owner.get(&parcel.drop)) {
            (None, None) => {}
            (Some(&(taxi_a, at)), Some(&(taxi_b, later))) => {
                if taxi_a != taxi_b || at >= later {
                    return invalid(format!("parcel {j} ownership or precedence violation"));
                }
            }
            _ => return invalid(format!("parcel {j} is partially served")),
        }
    }

    let (mut revenue, mut travel_cost) = (0i64, 0i64);
    let (mut served_passengers, mut served_parcels) = (0usize, 0usize);
    for (taxi, route) in routes.iter().enumerate() {
        let (mut departure, mut load) = (0i64, 0i64);
        let mut previous = instance.origins[taxi];
        for &node in route {
            let arrival = add(departure, instance.time(previous, node), "time")?;
            travel_cost = add(travel_cost, instance.cost(previous, node), "travel cost")?;
            let event = instance.events[&node];
            match event.kind {
                Kind::Passenger => {
                    let data = &instance.passengers[event.request];
                    if event.pickup {
                        if arrival > data.latest {
                            return invalid(format!(
                                "passenger {} pickup misses latest",
                                event.request
                            ));
                        }
                        departure = add(arrival.max(data.earliest), data.service, "time")?;
                        revenue = add(revenue, data.revenue, "revenue")?;
                        served_passengers += 1;
                    } else {
                        departure = add(arrival, data.service, "time")?;
                    }
                }
                Kind::Parcel => {
                    let data = &instance.parcels[event.request];
                    if event.pickup {
                        if arrival > data.pickup_latest {
                            return invalid(format!(
                                "parcel {} pickup misses latest",
                                event.request
                            ));
                        }
                        load = add(load, data.weight, "load")?;
                        if load > instance.capacity[taxi] {
                            return invalid(format!(
                                "parcel {} exceeds taxi capacity",
                                event.request
                            ));
                        }
                        departure = add(arrival.max(data.pickup_earliest), data.service, "time")?;
                        revenue = add(revenue, data.revenue, "revenue")?;
                        served_parcels += 1;
                    } else {
                        if arrival > data.drop_latest {
                            return invalid(format!(
                                "parcel {} delivery misses latest",
                                event.request
                            ));
                        }
                        if load < data.weight {
                            return invalid(format!(
                                "parcel {} delivery has negative load",
                                event.request
                            ));
                        }
                        load -= data.weight;
                        departure = add(arrival.max(data.drop_earliest), data.service, "time")?;
                    }
                }
            }
            previous = node;
        }
        if load != 0 {
            return invalid(format!("taxi {taxi} finishes with nonzero parcel load"));
        }
    }

    let Some(benefit) = revenue.checked_sub(travel_cost) else {
        return invalid("benefit overflow");
    };
    Ok(Summary {
        benefit,
        revenue,
        travel_cost,
        passengers: served_passengers,
        parcels: served_parcels,
    })
}

/// Reads both files, simulates the solution and compares the declared benefit.
pub fn check_files(instance_path: &Path, solution_path: &Path) -> Result<Summary> {
    let instance = read_instance(instance_path)?;
    let (declared, routes) = read_solution(solution_path, &instance)?;
    let summary = evaluate(&instance, &routes)?;
    if declared != summary.benefit {
        return invalid(format!(
            "declared benefit {declared} != computed benefit {}",
            summary.benefit
        ));
    }
    Ok(summary)
}
