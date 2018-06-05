ODR-DabMod Web UI
=================

Goals
-----

Enable users to play with digital predistortion settings, through a
visualisation of the settings and the parameters.

Make it easier to discover the tuning possibilities of the modulator.


Install
-------

Install dependencies: cherrypy and jinja2 python modules

Run
---

1. Execute ODR-DabMod, configured with zmq rc on port 9400
1. `cd gui`
1. `./run.py`
1. Connect your browser to `http://localhost:8099`

Todo
----

* Integrate DPDCE
  * Show DPD settings and effect visually
  * Allow load/store of DPD settings
* Use Feedback Server interface and make spectrum and constellation plots
* Get authentication to work
* Read and write config file, and add forms to change ODR-DabMod configuration
* Connect to supervisord to be able to restart ODR-DabMod
* Create a status page
  * Is process running?
  * Is modulator rate within bounds?
  * Are there underruns or late packets?
  * Is the GPSDO ok? (needs new RC params)
* Think about how to show PA settings
  * Return loss is an important metric
  * Some PAs offer serial interfaces for supervision

