# Building PES-VCS — A Version Control System from Scratch

**Name: R Muralidharan**

**SRN:PES2UG24CS387**

**Section:G**

**Objective:** Build a local version control system that tracks file changes, stores snapshots efficiently, and supports commit history. Every component maps directly to operating system and filesystem concepts.

**Platform:** Ubuntu 22.04

---

## Getting Started

### Prerequisites

```bash
sudo apt update && sudo apt install -y gcc build-essential libssl-dev
```

### Using This Repository

This is a **template repository**. Do **not** fork it.

1. Click **"Use this template"** → **"Create a new repository"** on GitHub
2. Name your repository (e.g., `SRN-pes-vcs`) and set it to **public**. Replace `SRN` with your actual SRN, e.g., `PESXUG24CSYYY-pes-vcs`
3. Clone this repository to your local machine and do all your lab work inside this directory.
4.  **Important:** Remember to commit frequently as you progress. You are required to have a minimum of 5 detailed commits per phase. Refer to [Submission Requirements](#submission-requirements) for more details.
5. Clone your new repository and start working

The repository contains skeleton source files with `// TODO` markers where you need to write code. Functions marked `// PROVIDED` are complete — do not modify them.

### Building

```bash
make          # Build the pes binary
make all      # Build pes + test binaries
make clean    # Remove all build artifacts
```

### Author Configuration

PES-VCS reads the author name from the `PES_AUTHOR` environment variable:

```bash
export PES_AUTHOR="Your Name <PESXUG24CS042>"
```

If unset, it defaults to `"PES User <pes@localhost>"`.

### File Inventory

| File               | Role                                 | Your Task                                          |
| ------------------ | ------------------------------------ | -------------------------------------------------- |
| `pes.h`            | Core data structures and constants   | Do not modify                                      |
| `object.c`         | Content-addressable object store     | Implement `object_write`, `object_read`            |
| `tree.h`           | Tree object interface                | Do not modify                                      |
| `tree.c`           | Tree serialization and construction  | Implement `tree_from_index`                        |
| `index.h`          | Staging area interface               | Do not modify                                      |
| `index.c`          | Staging area (text-based index file) | Implement `index_load`, `index_save`, `index_add`  |
| `commit.h`         | Commit object interface              | Do not modify                                      |
| `commit.c`         | Commit creation and history          | Implement `commit_create`                          |
| `pes.c`            | CLI entry point and command dispatch | Do not modify                                      |
| `test_objects.c`   | Phase 1 test program                 | Do not modify                                      |
| `test_tree.c`      | Phase 2 test program                 | Do not modify                                      |
| `test_sequence.sh` | End-to-end integration test          | Do not modify                                      |
| `Makefile`         | Build system                         | Do not modify                                      |

---



## Phase 1: Object Storage Foundation

**Filesystem Concepts:** Content-addressable storage, directory sharding, atomic writes, hashing for integrity

**Files:** `pes.h` (read), `object.c` (implement `object_write` and `object_read`)

### What to Implement

Open `object.c`. Two functions are marked `// TODO`:

1. **`object_write`** — Stores data in the object store.
   - Prepends a type header (`"blob <size>\0"`, `"tree <size>\0"`, or `"commit <size>\0"`)
   - Computes SHA-256 of the full object (header + data)
   - Writes atomically using the temp-file-then-rename pattern
   - Shards into subdirectories by first 2 hex chars of hash

2. **`object_read`** — Retrieves and verifies data from the object store.
   - Reads the file, parses the header to extract type and size
   - **Verifies integrity** by recomputing the hash and comparing to the filename
   - Returns the data portion (after the `\0`)

Read the detailed step-by-step comments in `object.c` before starting.

### Testing

```bash
make test_objects
./test_objects
```

The test program verifies:
- Blob storage and retrieval (write, read back, compare)
- Deduplication (same content → same hash → stored once)
- Integrity checking (detects corrupted objects)

**📸 Screenshot 1A:** Output of `./test_objects` showing all tests passing.
<img width="1366" height="292" alt="image" src="https://github.com/user-attachments/assets/ca6d380d-5f55-4c17-bf8a-3bfdbef9e220" />



**📸 Screenshot 1B:** `find .pes/objects -type f` showing the sharded directory structure.
<img width="1366" height="292" alt="image" src="https://github.com/user-attachments/assets/6efa7787-eb3b-43c8-b508-ae3b44801015" />

---

## Phase 2: Tree Objects

**Filesystem Concepts:** Directory representation, recursive structures, file modes and permissions

**Files:** `tree.h` (read), `tree.c` (implement all TODO functions)

### What to Implement

Open `tree.c`. Implement the function marked `// TODO`:

1. **`tree_from_index`** — Builds a tree hierarchy from the index.
   - Handles nested paths: `"src/main.c"` must create a `src` subtree
   - This is what `pes commit` uses to create the snapshot
   - Writes all tree objects to the object store and returns the root hash

### Testing

```bash
make test_tree
./test_tree
```

The test program verifies:
- Serialize → parse roundtrip preserves entries, modes, and hashes
- Deterministic serialization (same entries in any order → identical output)

**📸 Screenshot 2A:** Output of `./test_tree` showing all tests passing.
<img width="1459" height="565" alt="image" src="https://github.com/user-attachments/assets/817750df-9b55-4d8d-86cf-66b080e9dc73" />

**📸 Screenshot 2B:** Pick a tree object from `find .pes/objects -type f` and run `xxd .pes/objects/XX/YYY... | head -20` to show the raw binary format.
<img width="1522" height="210" alt="image" src="https://github.com/user-attachments/assets/f225fcdf-2882-43b3-bee0-f1d408fc9791" />

---

## Phase 3: The Index (Staging Area)

**Filesystem Concepts:** File format design, atomic writes, change detection using metadata

**Files:** `index.h` (read), `index.c` (implement all TODO functions)

### What to Implement

Open `index.c`. Three functions are marked `// TODO`:

1. **`index_load`** — Reads the text-based `.pes/index` file into an `Index` struct.
   - If the file doesn't exist, initializes an empty index (this is not an error)
   - Parses each line: `<mode> <hash-hex> <mtime> <size> <path>`

2. **`index_save`** — Writes the index atomically (temp file + rename).
   - Sorts entries by path before writing
   - Uses `fsync()` on the temp file before renaming

3. **`index_add`** — Stages a file: reads it, writes blob to object store, updates index entry.
   - Use the provided `index_find` to check for an existing entry

`index_find` , `index_status` and `index_remove` are already implemented for you — read them to understand the index data structure before starting.

#### Expected Output of `pes status`

```
Staged changes:
  staged:     hello.txt
  staged:     src/main.c

Unstaged changes:
  modified:   README.md
  deleted:    old_file.txt

Untracked files:
  untracked:  notes.txt
```

If a section has no entries, print the header followed by `(nothing to show)`.

### Testing

```bash
make pes
./pes init
echo "hello" > file1.txt
echo "world" > file2.txt
./pes add file1.txt file2.txt
./pes status
cat .pes/index    # Human-readable text format
```

**📸 Screenshot 3A:** Run `./pes init`, `./pes add file1.txt file2.txt`, `./pes status` — show the output.
<img width="1234" height="779" alt="image" src="https://github.com/user-attachments/assets/12c1408b-6339-4ecb-9b0a-e72e108c5b09" />

**📸 Screenshot 3B:** `cat .pes/index` showing the text-format index with your entries.
<img width="1241" height="80" alt="image" src="https://github.com/user-attachments/assets/c0451f0a-2c16-437a-a627-c122d5173bb8" />

---

## Phase 4: Commits and History

**Filesystem Concepts:** Linked structures on disk, reference files, atomic pointer updates

**Files:** `commit.h` (read), `commit.c` (implement all TODO functions)

### What to Implement

Open `commit.c`. One function is marked `// TODO`:

1. **`commit_create`** — The main commit function:
   - Builds a tree from the index using `tree_from_index()` (**not** from the working directory — commits snapshot the staged state)
   - Reads current HEAD as the parent (may not exist for first commit)
   - Gets the author string from `pes_author()` (defined in `pes.h`)
   - Writes the commit object, then updates HEAD

`commit_parse`, `commit_serialize`, `commit_walk`, `head_read`, and `head_update` are already implemented — read them to understand the commit format before writing `commit_create`.

The commit text format is specified in the comment at the top of `commit.c`.

### Testing

```bash
./pes init
echo "Hello" > hello.txt
./pes add hello.txt
./pes commit -m "Initial commit"

echo "World" >> hello.txt
./pes add hello.txt
./pes commit -m "Add world"

echo "Goodbye" > bye.txt
./pes add bye.txt
./pes commit -m "Add farewell"

./pes log
```

You can also run the full integration test:

```bash
make test-integration
```

**📸 Screenshot 4A:** Output of `./pes log` showing three commits with hashes, authors, timestamps, and messages.
<img width="852" height="652" alt="image" src="https://github.com/user-attachments/assets/2615701c-0855-4dcc-8664-57e337f150a9" />

**📸 Screenshot 4B:** `find .pes -type f | sort` showing object store growth after three commits.
<img width="1017" height="296" alt="image" src="https://github.com/user-attachments/assets/38bdd0c4-6b5e-4fe3-9ad2-2e6051774c33" />

**📸 Screenshot 4C:** `cat .pes/refs/heads/main` and `cat .pes/HEAD` showing the reference chain.
<img width="1034" height="70" alt="image" src="https://github.com/user-attachments/assets/ed8ea084-4527-4b17-9874-fdd9929aa879" />

---

## Phase 5 & 6: Analysis-Only Questions
Phase 5

Q5.1: A branch is a pointer file under .pes/refs/heads/<branch>, and HEAD usually stores ref: refs/heads/<branch>.

To implement pes checkout <branch>:

Validate branch exists: read .pes/refs/heads/<branch> and parse commit hash.
Resolve target commit and its tree by reading commit object, then tree object(s).
Check for dirty/conflicting working state before overwriting files.
Update working directory to match target tree:
Create/update tracked files from target blobs.
Remove tracked files that do not exist in target tree.
Create/remove directories as needed.
Restore executable bit from tree mode (100755 vs 100644).
Rewrite index to match checked-out tree snapshot.
Update HEAD:
Normal checkout: write ref: refs/heads/<branch> to .pes/HEAD.
Why this is complex:

It is not just pointer movement; it is a full filesystem transformation with safety checks.
You must preserve user data by detecting conflicts with local changes.
Nested trees require recursive traversal and path reconstruction.
Correct metadata (mode, deletions, directory cleanup) must be handled atomically enough to avoid partial states.


Q5.2: Conflict detection using index + object store:

Let H be current HEAD tree and T be target branch tree.
For each tracked path in index:
Compare working file metadata/content to index entry.
If different, path is locally dirty.
For each dirty path, check whether switching branches would touch it:
Path differs between H and T (added/removed/changed blob hash or mode), or
Path exists in working tree but target needs to write/delete at that path.
If both true (dirty + would-be-overwritten), refuse checkout and print conflicting paths.
Practical check:

Fast path: metadata compare (mtime, size) against index.
Safe path: recompute blob hash from working file and compare with index hash before deciding clean/dirty.
This mirrors Git behavior: checkout aborts when local uncommitted work would be lost.



Q5.3: Detached HEAD means .pes/HEAD contains a commit hash directly instead of a branch ref.

If you commit in detached HEAD:

New commits are created normally, but no branch file advances.
HEAD moves along this anonymous chain.
These commits are easy to lose later (unreachable after switching away) unless referenced.
Recovery options:

Immediately create a branch pointing to current detached commit:
write hash to .pes/refs/heads/<new-branch>
set HEAD to ref: refs/heads/<new-branch>
If already switched away, recover from reflog-like history (if implemented) or any saved commit hash and create a branch at that hash.

Phase 6


Q6.1: Algorithm to delete unreachable objects (mark-and-sweep):

Build root set of reachable commits:
all hashes in .pes/refs/heads/*
detached HEAD hash if applicable
optional safety roots (in-progress operations, reflog entries)
Mark phase (graph traversal):
For each root commit, DFS/BFS:
mark commit hash
follow parent commit hash(es)
follow tree hash
For each marked tree:
parse entries
mark child tree hashes and blob hashes
Sweep phase:
enumerate .pes/objects/**
delete any object whose hash is not in reachable set
Data structure:

Hash set of object IDs for O(1) membership checks.
Queue/stack for traversal.
Scale estimate (100,000 commits, 50 branches):

Commits visited is at most number of unique commits reachable, not 50 x 100,000.
In worst case, about 100,000 commits plus all referenced trees/blobs.
Total visited objects can be millions if each commit introduces many unique trees/blobs.


Q6.2: Why concurrent GC with commit is dangerous:

Race example:

Commit process writes new tree/blob objects first.
Before it writes commit object and updates branch ref, GC starts mark from refs.
New objects are not yet reachable from any ref, so GC classifies them unreachable.
GC deletes those objects.
Commit then writes ref to commit that points to now-missing objects, corrupting history.
How real Git avoids this (high level):

Uses reachability protections from temporary refs and reflogs.
Uses object mtime grace periods (recent objects are kept).
Coordinates maintenance with repository locking and safe operation windows.
Runs prune conservatively to avoid deleting objects that may become referenced imminently.

-----------

## Submission Requirements

**1. GitHub Repository**
* You must submit the link to your GitHub repository via the official submission link (which will be shared by your respective faculty).
* The repository must strictly maintain the directory structure you built throughout this lab.
* Ensure your github repository is made `public`

**2. Lab Report**
* Your report, containing all required **screenshots** and answers to the **analysis questions**, must be placed at the **root** of your repository directory.
* The report must be submitted as either a PDF (`report.pdf`) or a Markdown file (`README.md`).

**3. Commit History (Graded Requirement)**
* **Minimum Requirement:** You must have a minimum of **5 commits per phase** with appropriate commit messages. Submitting fewer than 5 commits for any given phase will result in a deduction of marks.
* **Best Practices:** We highly prefer more than 5 detailed commits per phase. Granular commits that clearly show the delta in code block changes allow us to verify your step-by-step understanding of the concepts and prevent penalties <3

---

## Further Reading

- **Git Internals** (Pro Git book): https://git-scm.com/book/en/v2/Git-Internals-Plumbing-and-Porcelain
- **Git from the inside out**: https://codewords.recurse.com/issues/two/git-from-the-inside-out
- **The Git Parable**: https://tom.preston-werner.com/2009/05/19/the-git-parable.html
