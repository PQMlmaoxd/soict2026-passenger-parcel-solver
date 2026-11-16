//! Instance model and parser for the contest input format.
//!
//! The file lists, as whitespace separated integers: `K N M`; one
//! `depot capacity` pair per taxi; `N` passenger lines
//! `pickup drop earliest latest service revenue`; `M` parcel lines
//! `pickup drop weight earliest latest dropEarliest dropLatest service revenue`;
//! then the `V x V` travel-time matrix and the `V x V` travel-cost matrix with
//! `V = K + 2N + 2M`. Vertices are 1-based.

use std::collections::HashMap;
use std::fmt;
use std::fs;
use std::io;
use std::path::Path;

/// Why a check could not succeed.
#[derive(Debug)]
pub enum Failure {
    /// The input or the solution breaks a rule of the contest.
    Invalid(String),
    /// A file could not be read.
    Unreadable(String),
}

impl fmt::Display for Failure {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Failure::Invalid(message) | Failure::Unreadable(message) => f.write_str(message),
        }
    }
}

impl std::error::Error for Failure {}

impl From<io::Error> for Failure {
    fn from(error: io::Error) -> Self {
        Failure::Unreadable(error.to_string())
    }
}

pub type Result<T> = std::result::Result<T, Failure>;

/// Shorthand for an [`Failure::Invalid`] result.
pub fn invalid<T>(message: impl Into<String>) -> Result<T> {
    Err(Failure::Invalid(message.into()))
}

#[derive(Debug, Clone)]
pub struct Passenger {
    pub pickup: usize,
    pub drop: usize,
    pub earliest: i64,
    pub latest: i64,
    pub service: i64,
    pub revenue: i64,
}

#[derive(Debug, Clone)]
pub struct Parcel {
    pub pickup: usize,
    pub drop: usize,
    pub weight: i64,
    pub pickup_earliest: i64,
    pub pickup_latest: i64,
    pub drop_earliest: i64,
    pub drop_latest: i64,
    pub service: i64,
    pub revenue: i64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Kind {
    Passenger,
    Parcel,
}

/// What happens at a service vertex: which request, and whether it is the pickup.
#[derive(Debug, Clone, Copy)]
pub struct Event {
    pub kind: Kind,
    pub request: usize,
    pub pickup: bool,
}

#[derive(Debug)]
pub struct Instance {
    pub taxis: usize,
    pub vertices: usize,
    pub passengers: Vec<Passenger>,
    pub parcels: Vec<Parcel>,
    /// Depot vertex of each taxi (1-based).
    pub origins: Vec<usize>,
    pub capacity: Vec<i64>,
    travel_time: Vec<i64>,
    travel_cost: Vec<i64>,
    /// Service vertex (1-based) to what happens there.
    pub events: HashMap<usize, Event>,
}

impl Instance {
    /// Travel time between two 1-based vertices.
    pub fn time(&self, from: usize, to: usize) -> i64 {
        self.travel_time[(from - 1) * self.vertices + (to - 1)]
    }

    /// Travel cost between two 1-based vertices.
    pub fn cost(&self, from: usize, to: usize) -> i64 {
        self.travel_cost[(from - 1) * self.vertices + (to - 1)]
    }
}

/// Integer tokens of a file, with the field name kept for error messages.
struct Tokens<'a> {
    values: Vec<&'a [u8]>,
    index: usize,
}

impl<'a> Tokens<'a> {
    fn new(data: &'a [u8]) -> Self {
        Tokens {
            values: data
                .split(|byte| byte.is_ascii_whitespace())
                .filter(|token| !token.is_empty())
                .collect(),
            index: 0,
        }
    }

    fn remaining(&self) -> usize {
        self.values.len() - self.index
    }

    fn take(&mut self, field: &str) -> Result<i64> {
        let Some(token) = self.values.get(self.index) else {
            return invalid(format!("missing {field}"));
        };
        self.index += 1;
        parse_integer(token, field)
    }
}

/// Parses a signed decimal integer and rejects anything else.
pub fn parse_integer(token: &[u8], field: &str) -> Result<i64> {
    let digits = token
        .strip_prefix(b"+")
        .or_else(|| token.strip_prefix(b"-"))
        .unwrap_or(token);
    if digits.is_empty() || !digits.iter().all(u8::is_ascii_digit) {
        return invalid(format!("invalid {field}"));
    }
    // The bytes were just verified to be ASCII digits and an optional sign.
    let text = std::str::from_utf8(token).expect("ascii token");
    match text.parse::<i64>() {
        Ok(value) => Ok(value),
        Err(_) => invalid(format!("{field} outside signed 64-bit range")),
    }
}

fn vertex(value: i64, count: usize, field: &str) -> Result<usize> {
    if value < 1 || value as u64 > count as u64 {
        return invalid(format!("invalid {field} vertex"));
    }
    Ok(value as usize)
}

fn nonnegative(value: i64, field: &str) -> Result<i64> {
    if value < 0 {
        return invalid(format!("invalid {field}"));
    }
    Ok(value)
}

/// Reads and validates an instance file.
pub fn read_instance(path: &Path) -> Result<Instance> {
    let data = fs::read(path)?;
    let mut tokens = Tokens::new(&data);
    let k = tokens.take("K")?;
    let n = tokens.take("N")?;
    let m = tokens.take("M")?;
    if k <= 0 || n < 0 || m < 0 {
        return invalid("invalid K/N/M");
    }
    let (k, n, m) = (k as usize, n as usize, m as usize);
    let v = k + 2 * n + 2 * m;
    if v > i32::MAX as usize {
        return invalid("invalid V dimension");
    }

    let mut origins = Vec::with_capacity(k);
    let mut capacity = Vec::with_capacity(k);
    for taxi in 0..k {
        origins.push(vertex(
            tokens.take(&format!("origin[{taxi}]"))?,
            v,
            "origin",
        )?);
        capacity.push(nonnegative(
            tokens.take(&format!("capacity[{taxi}]"))?,
            "capacity",
        )?);
    }

    let mut events = HashMap::new();
    let mut add_event = |node: usize, kind: Kind, request: usize, pickup: bool| -> Result<()> {
        if events
            .insert(
                node,
                Event {
                    kind,
                    request,
                    pickup,
                },
            )
            .is_some()
        {
            return invalid(format!(
                "ambiguous shared service endpoint at vertex {node}"
            ));
        }
        Ok(())
    };

    let mut passengers = Vec::with_capacity(n);
    for i in 0..n {
        let mut field = |name: &str| tokens.take(&format!("passenger[{i}].{name}"));
        let pickup = vertex(field("pickup")?, v, "passenger pickup")?;
        let drop = vertex(field("drop")?, v, "passenger drop")?;
        let (earliest, latest) = (field("earliest")?, field("latest")?);
        let (service, revenue) = (field("service")?, field("revenue")?);
        if earliest < 0 || latest < earliest || service < 0 || revenue < 0 {
            return invalid(format!("invalid passenger[{i}] window or value"));
        }
        add_event(pickup, Kind::Passenger, i, true)?;
        add_event(drop, Kind::Passenger, i, false)?;
        passengers.push(Passenger {
            pickup,
            drop,
            earliest,
            latest,
            service,
            revenue,
        });
    }

    let mut parcels = Vec::with_capacity(m);
    for j in 0..m {
        let mut field = |name: &str| tokens.take(&format!("parcel[{j}].{name}"));
        let pickup = vertex(field("pickup")?, v, "parcel pickup")?;
        let drop = vertex(field("drop")?, v, "parcel drop")?;
        let weight = field("weight")?;
        let (pickup_earliest, pickup_latest) = (field("pickup_earliest")?, field("pickup_latest")?);
        let (drop_earliest, drop_latest) = (field("drop_earliest")?, field("drop_latest")?);
        let (service, revenue) = (field("service")?, field("revenue")?);
        if weight < 0
            || pickup_earliest < 0
            || pickup_latest < pickup_earliest
            || drop_earliest < 0
            || drop_latest < drop_earliest
            || service < 0
            || revenue < 0
        {
            return invalid(format!("invalid parcel[{j}] window or value"));
        }
        add_event(pickup, Kind::Parcel, j, true)?;
        add_event(drop, Kind::Parcel, j, false)?;
        parcels.push(Parcel {
            pickup,
            drop,
            weight,
            pickup_earliest,
            pickup_latest,
            drop_earliest,
            drop_latest,
            service,
            revenue,
        });
    }

    if tokens.remaining() != 2 * v * v {
        return invalid("matrix size or trailing input data is invalid");
    }
    let mut matrix = |field: &str| -> Result<Vec<i64>> {
        (0..v * v)
            .map(|_| nonnegative(tokens.take(field)?, field))
            .collect()
    };
    let travel_time = matrix("travel time")?;
    let travel_cost = matrix("travel cost")?;

    Ok(Instance {
        taxis: k,
        vertices: v,
        passengers,
        parcels,
        origins,
        capacity,
        travel_time,
        travel_cost,
        events,
    })
}
