//! Synthetic instances in the contest input format.
//!
//! Vertices are points in the plane and travel times follow Euclidean distance.
//! An asymmetry factor perturbs time and cost per arc, so the matrices are
//! directed and need not satisfy the triangle inequality. Costs are 2.45 times
//! the perturbed distance. Revenue is a random multiple of the direct trip cost,
//! so some requests only pay off when they fit well into a route.
//!
//! Layouts:
//! * `uni`: vertices uniform in a 100 x 100 square
//! * `clu`: eight Gaussian clusters
//! * `hub`: a dense centre holding 60 % of the vertices and a sparse ring
//!
//! The output is a pure function of the parameters. The order of the random
//! calls below defines the stream: do not reorder them.

use std::fmt::Write;

use crate::mt::PyRandom;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Layout {
    Uniform,
    Clustered,
    Hub,
}

impl Layout {
    pub fn parse(name: &str) -> Option<Layout> {
        match name {
            "uni" => Some(Layout::Uniform),
            "clu" => Some(Layout::Clustered),
            "hub" => Some(Layout::Hub),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct Params {
    pub taxis: usize,
    pub passengers: usize,
    pub parcels: usize,
    pub seed: u64,
    pub layout: Layout,
    /// Horizon of the earliest start times.
    pub horizon: f64,
    /// Scale of the time windows.
    pub window: f64,
    /// Arc asymmetry in `[0, 1]`.
    pub asymmetry: f64,
}

type Point = (f64, f64);

fn place_vertices(rnd: &mut PyRandom, layout: Layout, count: usize) -> Vec<Point> {
    match layout {
        Layout::Uniform => (0..count)
            .map(|_| (rnd.uniform(0.0, 100.0), rnd.uniform(0.0, 100.0)))
            .collect(),
        Layout::Clustered => {
            let centres: Vec<Point> = (0..8)
                .map(|_| (rnd.uniform(10.0, 90.0), rnd.uniform(10.0, 90.0)))
                .collect();
            (0..count)
                .map(|_| {
                    let (cx, cy) = centres[rnd.choice_index(centres.len())];
                    (cx + rnd.gauss(0.0, 6.0), cy + rnd.gauss(0.0, 6.0))
                })
                .collect()
        }
        Layout::Hub => (0..count)
            .map(|_| {
                if rnd.random() < 0.6 {
                    (50.0 + rnd.gauss(0.0, 8.0), 50.0 + rnd.gauss(0.0, 8.0))
                } else {
                    let angle = rnd.uniform(0.0, 2.0 * std::f64::consts::PI);
                    let radius = rnd.uniform(30.0, 50.0);
                    (50.0 + radius * angle.cos(), 50.0 + radius * angle.sin())
                }
            })
            .collect(),
    }
}

/// Python's `round`: to the nearest integer, ties to even.
fn round(value: f64) -> i64 {
    value.round_ties_even() as i64
}

fn build_matrix(rnd: &mut PyRandom, points: &[Point], scale: f64, asymmetry: f64) -> Vec<i64> {
    let size = points.len();
    let mut matrix = vec![0i64; size * size];
    for i in 0..size {
        for j in 0..size {
            if i != j {
                let distance = (points[i].0 - points[j].0).hypot(points[i].1 - points[j].1);
                let noise = 1.0 + asymmetry * rnd.uniform(-0.3, 0.6);
                matrix[i * size + j] = round(scale * distance * noise).max(1);
            }
        }
    }
    matrix
}

/// Builds the instance text for the given parameters.
pub fn generate(params: &Params) -> String {
    let Params {
        taxis,
        passengers,
        parcels,
        seed,
        layout,
        horizon,
        window,
        asymmetry,
    } = *params;
    let mut rnd = PyRandom::new(seed);
    let requests = passengers + parcels;
    let vertices = 2 * requests + taxis;
    let points = place_vertices(&mut rnd, layout, vertices);
    let time = build_matrix(&mut rnd, &points, 1.0, asymmetry);
    let cost = build_matrix(&mut rnd, &points, 2.45, asymmetry);

    let mut text = String::new();
    writeln!(text, "{taxis} {passengers} {parcels}").unwrap();
    for k in 0..taxis {
        writeln!(text, "{} {}", k + 1, rnd.randint(10, 40)).unwrap();
    }
    for r in 0..requests {
        let pickup = taxis + 2 * r;
        let drop = pickup + 1;
        let service = rnd.randint(1, 6);
        let direct_cost = cost[pickup * vertices + drop] as f64;
        let by_cost = direct_cost * rnd.uniform(1.3, 2.6);
        let revenue = round(by_cost + rnd.uniform(0.0, 15.0)).max(1);
        if r < passengers {
            let earliest = round(rnd.uniform(0.0, horizon));
            let latest = earliest + round(window * rnd.uniform(5.0, 25.0));
            writeln!(
                text,
                "{} {} {earliest} {latest} {service} {revenue}",
                pickup + 1,
                drop + 1
            )
            .unwrap();
        } else {
            let earliest = round(rnd.uniform(0.0, 0.9 * horizon));
            let latest = earliest + round(window * rnd.uniform(5.0, 35.0));
            let drop_earliest = earliest + time[pickup * vertices + drop] + rnd.randint(2, 15);
            let drop_latest = drop_earliest + round(window * rnd.uniform(12.0, 70.0));
            let weight = rnd.randint(3, 15);
            writeln!(
                text,
                "{} {} {weight} {earliest} {latest} {drop_earliest} {drop_latest} {service} {revenue}",
                pickup + 1,
                drop + 1
            )
            .unwrap();
        }
    }
    for matrix in [&time, &cost] {
        for row in matrix.chunks(vertices) {
            let line: Vec<String> = row.iter().map(i64::to_string).collect();
            writeln!(text, "{}", line.join(" ")).unwrap();
        }
    }
    text
}
