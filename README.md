DistributiOn aNd mAnagemenT sErver:
---------------

this is a first protoype for a set of tools to connect processes via pipes in complicated constellations.
It works by using unix domain sockets to distribute data, which in ingested via the minipub tool, by piping to its stdin and will be received by the minisub tool, which pushes it to its own stdout while passing all stdin through.

thus many independend processes can easily make data available to other processes, completely lanugage angostic and insensitive to single process crashes.

this is intended to be used as the base protocoll for modular daq, live monitoring and controll setups for various experiments.

cross device communication is the next step, but still needs to be implemented.
