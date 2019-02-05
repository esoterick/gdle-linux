# GDLEnhanced

Asheron's Call Emulator based on the GDL project, and GDL-Classic Dereth projects

* GDL Github: <https://github.com/GamesDeadLol/GDL>
* GDL-Classic Github: <https://github.com/bDekaru/ClassicDereth>

## Maintained by the GDLE Development Team

Contact us on Discord: <https://discord.gg/WzGX348>

---

### Development

* Scribble
* Chosen One
* LikeableLime
* Morosity
* fourk
* scruples
* drunkferret
* mwnciau
* fdsfsd
* Auning

### Content

* shark
* Targin
* Zarto
* SeraphinX
* Crimson Mage
* Lord Traithia
* Mentel
* Zykot
* Zeus
* Shadow King Recks
* birbistheworb

### Additional support

* gmriggs
* The golems
* Kloud

## Installation

__GDLE Setup Guide__ _Updated January 19, 2019_

### Pre-requisites

__This release of the server has been tested to run on Windows 10 64-bit, but may run on others.__

The hardware requirements depend upon the number of players. Large memory use is intentional and should be expected: a minimum of 32GB of RAM is recommended for long-term use with many players. It is recommended that any server intending to host many players have a very fast internet connection (fiber), multi-core machine with good single-threaded performance, installed on a fast drive (SSD.)

* Asherons Call Client files from January 31, 2017 [See Hashes](#file-hashes)
* MySQL/MariaDB
* Microsoft Visual Stuido

### Intermediate Guide

__This release is most suitable for technically comfortable users. There is an intermediate and advanced guide below.__

1. Build the project in Visual Studio
2. Copy `client_portal.dat` and `client_cell_1.dat` files to the `Bin\Data` folder (e.g. `C:\GDLEServer\Bin\Data`)
3. Copy your all of your game files __(all of them)__ to the `Client` folder (e.g. `C:\GDLEServer\Bin\Client`)
4. GDLE requires MySQL or MariaDB for database purposes. The easiest way to install them is this:

    1. Download, then install [WAMP 3.0.6 64-bit](http://wampserver.aviatechno.net/files/install/wampserver3.0.6_x64_apache2.4.23_mysql5.7.14_php5.6.25-7.0.10.exe)
    2. Download, then install  [WAMP Update](http://wampserver.aviatechno.net/files/updates/wampserver3_x86_x64_update3.0.9.exe)

        **Note: Change the install folder to make sure it is the same as the first one!**

    3. Download/install the [MariaDB addon for WAMP](http://wampserver.aviatechno.net/files/mariadb/wampserver3_x64_addon_mariadb10.1.23.exe)
5. Make sure WAMP is running, and go to http://localhost/phpmyadmin/ in your web browser. Login to PhpMyAdmin as `root` with no password
6. On the left side, click "New" and create a database named `GDLE`
7. With the newly created `GDLE` database selected, click the "Import" tab on the right side, select the `gdle_db.sql` file located in the `sqldumps` folder (e.g. `C:\GDLEServer\Bin\Data\sqldumps`) and click "Go"
    1. Repeat for `blob_update_house.sql`
    1. Repeat for `blob_update_weenie.sql`
8. Run the `GDLE.exe` program
9. Click Start
10. Click Launch, or use a launcher app such GDLE Launcher or ThwargLauncher to connect to the server
11. In order for others to connect to your server, you may need to set the `bind_ip` value to `0.0.0.0` located in the `server.cfg` file

### Advanced Setup

1. Clone the repository or extract all files from the ZIP to a folder such as (e.g. `C:\GDLEServer`)
2. Copy `client_portal.dat` and `client_cell_1.dat` files to the `Bin\Data` folder (e.g. `C:\GDLEServer\Bin\Data`)
3. Install MariaDB (10.1 tested to work well)
4. Import the `gdle_db.sql` file located in the `sqldumps` folder (e.g. `C:\GDLEServer\Bin\Data\sqldumps`)
    1. Repeat for `blob_update_house.sql`
    1. Repeat for `blob_update_weenie.sql`
5. Open the `server.cfg` file and alter it how you wish, for example you may wish to update the database port (`database_port=3311`)
6. Optionally, copy your game files completely to the `Client` folder so the "Launch" button will work in the GDLE Server UI (e.g. `C:\GDLEServer\Bin\Client`)
7. Run the GDLE server and click Start
8. Click Launch, or use a launcher app such GDLE Launcher or ThwargLauncher to connect to the server
9. In order for others to connect to your server, you may need to set the `bind_ip` value to `0.0.0.0` located in the `server.cfg` file

## Loading Weenies from Lifestoned.org

1. Go to Lifestoned.org's [World Releases](https://lifestoned.org/WorldRelease)
2. Download the latest release
3. Extract the contents in to the `Bin\Data\json\weenies` directory (e.g. `C:\GDLEServer\Bin\Data\json\weenies`)

## File Hashes

Make sure they are __FULL__ files from January 31, 2017

* client_portal.dat
  * __MD5__: `2C89662A44FCDC2A3C31FE8F6677D265`
  * __SHA1__: `7DBB04F8CC92483467D8CC327D2892FDCD38D764`
  * __SHA256__: `DC6E500BA22E6B186DB7171E3F3345238B6444C85D798ADC85E550973B8D12E4`

* client_cell_1.dat
  * __MD5__: `6401B73FD3842FFDB953339522A7331A`
  * __SHA1__: `807CBD7959C2775A55F2349EBCFB44F1937901FD`
  * __SHA256__: `6DB0ABF00FBCEED62C3F1EE842EE7C1F423D732BED77A5B7C102EE89A52AB99E`

## Update user with admin privilege

In the example we update the user we registered with the name `boatymcboatface` with admin privileges.

```sql
USE GDLE;
UPDATE accounts AS a
SET a.access = 6
WHERE username = "boatymcboatface";
```

_Good luck, have fun._