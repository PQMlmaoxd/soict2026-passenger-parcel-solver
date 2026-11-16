//! Mersenne Twister with the seeding and helpers of CPython's `random` module.
//!
//! The benchmark instances of this project were first produced by a Python
//! generator. Reproducing its random stream exactly keeps those instances, and
//! every result measured on them, reproducible without Python. Only the calls
//! the generator needs are implemented: `random`, `uniform`, `gauss`,
//! `randint` and `choice`.

const N: usize = 624;
const M: usize = 397;
const TWO_PI: f64 = 2.0 * std::f64::consts::PI;

pub struct PyRandom {
    state: [u32; N],
    index: usize,
    gauss_next: Option<f64>,
}

impl PyRandom {
    /// Seeds like `random.Random(seed)` for a non-negative integer seed.
    pub fn new(seed: u64) -> Self {
        let mut key = Vec::new();
        let mut rest = seed;
        loop {
            key.push((rest & 0xffff_ffff) as u32);
            rest >>= 32;
            if rest == 0 {
                break;
            }
        }
        let mut generator = PyRandom {
            state: [0; N],
            index: N,
            gauss_next: None,
        };
        generator.init_by_array(&key);
        generator
    }

    fn init_genrand(&mut self, seed: u32) {
        self.state[0] = seed;
        for i in 1..N {
            let previous = self.state[i - 1];
            self.state[i] = 1_812_433_253u32
                .wrapping_mul(previous ^ (previous >> 30))
                .wrapping_add(i as u32);
        }
        self.index = N;
    }

    fn init_by_array(&mut self, key: &[u32]) {
        self.init_genrand(19_650_218);
        let (mut i, mut j) = (1usize, 0usize);
        for _ in 0..N.max(key.len()) {
            let previous = self.state[i - 1];
            self.state[i] = (self.state[i] ^ (previous ^ (previous >> 30)).wrapping_mul(1_664_525))
                .wrapping_add(key[j])
                .wrapping_add(j as u32);
            i += 1;
            j += 1;
            if i >= N {
                self.state[0] = self.state[N - 1];
                i = 1;
            }
            if j >= key.len() {
                j = 0;
            }
        }
        for _ in 0..N - 1 {
            let previous = self.state[i - 1];
            self.state[i] = (self.state[i]
                ^ (previous ^ (previous >> 30)).wrapping_mul(1_566_083_941))
            .wrapping_sub(i as u32);
            i += 1;
            if i >= N {
                self.state[0] = self.state[N - 1];
                i = 1;
            }
        }
        self.state[0] = 0x8000_0000;
    }

    fn twist(&mut self) {
        for k in 0..N {
            let high = self.state[k] & 0x8000_0000;
            let low = self.state[(k + 1) % N] & 0x7fff_ffff;
            let mixed = high | low;
            let mut next = self.state[(k + M) % N] ^ (mixed >> 1);
            if mixed & 1 == 1 {
                next ^= 0x9908_b0df;
            }
            self.state[k] = next;
        }
        self.index = 0;
    }

    fn next_u32(&mut self) -> u32 {
        if self.index >= N {
            self.twist();
        }
        let mut y = self.state[self.index];
        self.index += 1;
        y ^= y >> 11;
        y ^= (y << 7) & 0x9d2c_5680;
        y ^= (y << 15) & 0xefc6_0000;
        y ^ (y >> 18)
    }

    /// Uniform float in `[0, 1)` with 53 random bits.
    pub fn random(&mut self) -> f64 {
        let a = f64::from(self.next_u32() >> 5);
        let b = f64::from(self.next_u32() >> 6);
        (a * 67_108_864.0 + b) * (1.0 / 9_007_199_254_740_992.0)
    }

    /// `random.uniform(a, b)`.
    pub fn uniform(&mut self, a: f64, b: f64) -> f64 {
        a + (b - a) * self.random()
    }

    /// `random.gauss(mu, sigma)`. Each pair of draws yields two values.
    pub fn gauss(&mut self, mu: f64, sigma: f64) -> f64 {
        let z = match self.gauss_next.take() {
            Some(cached) => cached,
            None => {
                let angle = self.random() * TWO_PI;
                let radius = (-2.0 * (1.0 - self.random()).ln()).sqrt();
                self.gauss_next = Some(angle.sin() * radius);
                angle.cos() * radius
            }
        };
        mu + z * sigma
    }

    /// Uniform integer in `[0, n)`, by rejection on the top bits like CPython.
    fn below(&mut self, n: u64) -> u64 {
        debug_assert!((1..=u64::from(u32::MAX)).contains(&n));
        let bits = 64 - n.leading_zeros();
        loop {
            let candidate = u64::from(self.next_u32() >> (32 - bits));
            if candidate < n {
                return candidate;
            }
        }
    }

    /// `random.randint(a, b)`, both ends included.
    pub fn randint(&mut self, a: i64, b: i64) -> i64 {
        a + self.below((b - a + 1) as u64) as i64
    }

    /// Index chosen by `random.choice` on a sequence of `len` items.
    pub fn choice_index(&mut self, len: usize) -> usize {
        self.below(len as u64) as usize
    }
}

#[cfg(test)]
mod tests {
    use super::PyRandom;

    // Reference values from CPython 3.11: random.Random(seed).random().
    #[test]
    fn matches_cpython_random() {
        let mut generator = PyRandom::new(42);
        assert_eq!(generator.random(), 0.6394267984578837);
        assert_eq!(generator.random(), 0.025010755222666936);
        let mut generator = PyRandom::new(1);
        assert_eq!(generator.random(), 0.13436424411240122);
    }

    #[test]
    fn randint_stays_in_range() {
        let mut generator = PyRandom::new(7);
        for _ in 0..1000 {
            let value = generator.randint(3, 15);
            assert!((3..=15).contains(&value));
        }
    }
}
