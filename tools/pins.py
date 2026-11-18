"""Where the checks fetch their firmware and test programs, and the hashes these
must have. None of it is part of ReC64 or of a build."""

ROMS = [{'key': 'kernal',
  'name': 'C64 KERNAL 901227-03',
  'bytes': 8192,
  'sha256': '83c60d47047d7beab8e5b7bf6f67f80daa088b7a6a27de0d7e016f6484042721',
  'urls': ['https://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/kernal.901227-03.bin']},
 {'key': 'basic',
  'name': 'C64 BASIC 901226-01',
  'bytes': 8192,
  'sha256': '89878cea0a268734696de11c4bae593eaaa506465d2029d619c0e0cbccdfa62d',
  'urls': ['https://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/basic.901226-01.bin']},
 {'key': 'chargen',
  'name': 'C64 character generator 901225-01',
  'bytes': 4096,
  'sha256': 'fd0d53b8480e86163ac98998976c72cc58d5dd8eb824ed7b829774e74213b420',
  'urls': ['https://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64/characters.901225-01.bin']},
 {'key': 'dos1541',
  'name': '1541 DOS 325302-01 + 901229-05',
  'bytes': 16384,
  'sha256': 'd1d45afb46fd4e2b48d93ca367b889d75654a3b7acf73044e51f6d880c09369e',
  'urls': ['https://www.zimmers.net/anonftp/pub/cbm/firmware/drives/new/1541/1541-c000.325302-01.bin',
           'https://www.zimmers.net/anonftp/pub/cbm/firmware/drives/new/1541/1541-e000.901229-05.bin']},
 {'key': 'dos1541ii',
  'name': '1541-II DOS 251968-03',
  'bytes': 16384,
  'sha256': '326c289c38753323d7e8167897447cf61ef35189d82eb8d75210ece949adda7c',
  'urls': ['https://www.zimmers.net/anonftp/pub/cbm/firmware/drives/new/1541/1541-II.251968-03.bin']}]

LORENZ = {'url': 'https://www.zimmers.net/anonftp/pub/cbm/documents/chipdata/tsuit215_d64.zip',
 'bytes': 169382,
 'sha256': '84a715ece5976ecf468570739130358a48e62ec8c31c2d3265f2d98ff4a067fc'}

KRILL_LOADER = {'url': 'https://csdb.dk/getinternalfile.php/170232/loader-v164.zip',
 'sha256': 'd41301555c9e53cc9e9000cf62f368bffab27f0ae9c68fb14bb600994b902838',
 'file': 'loader/build/loadertest-c64.d64',
 'file_sha256': 'e63e422b4fb3ed27309d1529b748aa6159b84ffdc7abfec34a8dc464408df799'}

FREEZER_FIRMWARE = {'action_replay': {'files': {'Action_Replay_MK4_1988_PAL.crt': {'sha256': '102be38a43a40637fd8397b621f830e15ec4243635bd037f54ac644502b43490'},
                             'Action_Replay_V4.1_4.1_1988_PAL.crt': {'sha256': '84da15985d444629b12d56da89ccebd046c18d70c9ccdb5cc0bfaa2cae18d773'},
                             'Action_Replay_V5.0_5.0_1988_PAL.crt': {'sha256': '1ebde415b025ef18f38a6cb2043ef510ed1f96e9f6b85f332f8b3dc7c73f2f79'},
                             'Action_Replay_V6.0_6.0_1989_PAL.crt': {'sha256': 'dffd3d3eea4f91e054671953fcc5bbb42cf9480b8b67fbf28221368db33e6618'}}},
 'final_cartridge_iii': {'sha256': '84ea3120b7f2a683e59a54f9d1eab85bd3a14c94657429b57c143fc89b897696'},
 'retro_replay': {'rom_sha256': '25b643b6dd3afb8c79c8b1ec8aa4c1fc292ae4a39e92d596a71b875d9b823727'}}

SID_TUNES = {'Commando.sid': {'sha256': '5295378dacb06593a9c1853ce11a325ce885a16e26d56274d424e06492948de8'},
 'Arkanoid.sid': {'sha256': '9a576463b6d9e46f35cf2c8fbf40b6b3915424b2207d7c44c73b36e6ce9bc1cd'}}
