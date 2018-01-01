# gitweb - API

Provides data (commits and files) from Git repositories as JSON inspired by the GitHub V3 APIs.

## Dependencies:
* libgit2

## Building

### On Alpine Linux

Install the packages:
* apk add make g++ libgit2-dev

Build it
* make

Run it
* ./gitjson /api/
* ./gitjson /api/repos/gitweb

NOTE: The base path for where to find repos was hard coded for windows and
will need to be changed. This is not by-design.

## License:
  Under the MIT license, see LICENSE.txt for details.

## Features:
 * Provides data from Git repositories as JSON mostly following the GitHub V3 APIs.
 * Host a HTTP server (via Python)

## JSON interface:
| URI           | Description   |
| ------------- |:-------------:|
| /api/repos/{repo-name} | Summary of that repo. |
| /api/repos/{repo-name}/branches | List the branches in that repo |
| /api/repos/{repo-name}/tags | List the tags in that repo |
| /api/repos/{repo-name}/tags/{name} | Information about that tag. |
| /api/repos/{repo-name}/commit/{hash} | Information for that hash.|

## TODO:
* Host a HTTP server in C++ (via Boost.Beast)
* Learn and document how to hook up this server to NGINX.
* ~~Generate a HTML pages for each commit (logs etc)~~ Leave this to web client.

##HTML pages:
The following is currently outside of the scope.
| URI           | Description   |
| ------------- |:-------------:|
| /             | Main screen   |
| /{repo-name}  | Home screen for a given repo |
| /{repo-name}/branches | List the branches in that repo  |
| /{repo-name}/tags     | List the tags in that repo      |
| /{repo-name}/commit/{hash} | Information for that hash. |
