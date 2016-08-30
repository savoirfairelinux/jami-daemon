# VideoManager API Standards

All of the routes declared in this page are accesible using the path /video/v1/[url]

Go see our [REST API Standards](https://github.com/sevaivanov/ring-api/wiki/REST-API-Standards)

## URL

### /devices/

#### REST Request

| METHOD | URL     |
|--------|---------|
|GET     | /devices/ |

#### Response
Returns a list of the detected v4l2 devices.

    {
		"devices" : [string]
    }

### /capabilities/[device]/

#### REST Request

| METHOD | URL     |
|--------|---------|
|GET     | /capabilities/[device]/ |

#### Response
Returns a map of map of array of strings, containing the capabilities (channel, size, rate) of a device.

    {
	    "capabilities" : {
			// TODO
		}
    }

### /settings/[device]/

#### REST Request

| METHOD | URL     |
|--------|---------|
|GET     | /settings/[device]/ |
|PUT 	 | /settings/[device]/ |

#### PUT Request
    {
      // TODO
    }

#### Response
##### GET
Returns a map of settings for the given device name

    {
	    "settings" : {
			string : string
		}
    }

### /default/[device]/

#### REST Request

| METHOD | URL     |
|--------|---------|
|GET     | /default/ |
|PUT 	 | /default/ |

#### POST Request
    {
      "device" : "deviceName"
    }

#### Response
##### GET
Returns the device used by default by the daemon

    {
      "default" : string
    }

### /camera/

#### REST Request

| METHOD | URL     |
|--------|---------|
|GET 	 | /camera/  |
|PUT 	 | /camera/  |

#### PUT Request
Start or stop the camera. True to start, false to stop.
	{
		"status" : bool
    }

#### Response
##### GET
Returns true if the camera has already started, false otherwise

	{
		"status" : bool
	}

### /switchInput/[resource]/

A media resource locator (MRL). Currently, the following are supported:

 * camera://DEVICE
 * display://DISPLAY_NAME[ WIDTHxHEIGHT]
 * file://IMAGE_PATH

#### REST Request

| METHOD | URL     |
|--------|---------|
|GET 	 | /switchInput/[resource]/  |

#### Response
Returns true if the input stream was successfully changed, false otherwise

    {
	    "status" : bool
    }
