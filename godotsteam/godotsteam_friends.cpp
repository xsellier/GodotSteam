#include "godotsteam_friends.h"

GodotSteamFriends *GodotSteamFriends::singleton = NULL;

GodotSteamFriends::GodotSteamFriends() { singleton = this; }
GodotSteamFriends::~GodotSteamFriends() { singleton = NULL; }

GodotSteamFriends *GodotSteamFriends::get_singleton() {
  if (GodotSteamFriends::singleton == NULL) {
    GodotSteamFriends::singleton = new GodotSteamFriends();
  }

  return GodotSteamFriends::singleton;
}

void GodotSteamFriends::reset_singleton() {
  delete GodotSteamFriends::singleton;

  GodotSteamFriends::singleton = NULL;
}

bool GodotSteamFriends::isSteamFriendsReady() { return SteamFriends() != NULL; }

int GodotSteamFriends::getFriendCount() {
  if (!isSteamFriendsReady()) {
    return 0;
  }

  return SteamFriends()->GetFriendCount(0x04);
}

String GodotSteamFriends::getPersonaName() {
  if (!isSteamFriendsReady()) {
    return "";
  }

  return SteamFriends()->GetPersonaName();
}

String GodotSteamFriends::getFriendPersonaName(int steamID) {
  String personaName = "";

  if (isSteamFriendsReady() && steamID > 0) {
    CSteamID friendID =
        GodotSteamUtils::get_singleton()->createSteamID(steamID);
    bool isDataLoading = SteamFriends()->RequestUserInformation(friendID, true);

    if (!isDataLoading) {
      personaName = SteamFriends()->GetFriendPersonaName(friendID);
    }
  }

  return personaName;
}

void GodotSteamFriends::setGameInfo(const String &s_key,
                                    const String &s_value) {
  // Rich presence data is automatically shared between friends in the same game
  // Each user has a set of key/value pairs, up to 20 can be set
  // Two magic keys (status, connect)
  // setGameInfo() to an empty string deletes the key
  if (!isSteamFriendsReady()) {
    return;
  }

  SteamFriends()->SetRichPresence(s_key.utf8().get_data(),
                                  s_value.utf8().get_data());
}

void GodotSteamFriends::clearGameInfo() {
  if (!isSteamFriendsReady()) {
    return;
  }

  SteamFriends()->ClearRichPresence();
}

void GodotSteamFriends::inviteFriend(int steamID, const String &conString) {
  if (!isSteamFriendsReady()) {
    return;
  }

  CSteamID friendID = GodotSteamUtils::get_singleton()->createSteamID(steamID);

  SteamFriends()->InviteUserToGame(friendID, conString.utf8().get_data());
}

void GodotSteamFriends::setPlayedWith(int steamID) {
  if (!isSteamFriendsReady()) {
    return;
  }

  CSteamID friendID = GodotSteamUtils::get_singleton()->createSteamID(steamID);
  SteamFriends()->SetPlayedWith(friendID);
}

Array GodotSteamFriends::getRecentPlayers() {
  if (!isSteamFriendsReady()) {
    return Array();
  }

  int rCount = SteamFriends()->GetCoplayFriendCount();
  Array recents;

  for (int index = 0; index < rCount; index++) {
    CSteamID rPlayerID = SteamFriends()->GetCoplayFriend(index);

    if (SteamFriends()->GetFriendCoplayGame(rPlayerID) ==
        SteamUtils()->GetAppID()) {
      Dictionary rPlayer;
      String rName = SteamFriends()->GetFriendPersonaName(rPlayerID);
      int rStatus = SteamFriends()->GetFriendPersonaState(rPlayerID);

      rPlayer["id"] = rPlayerID.GetAccountID();
      rPlayer["name"] = rName;
      rPlayer["status"] = rStatus;

      recents.append(rPlayer);
    }
  }

  return recents;
}

void GodotSteamFriends::getFriendAvatar(int size) {
  if (size < AVATAR_SMALL || size > AVATAR_LARGE) {
    return;
  }

  if (!isSteamFriendsReady()) {
    return;
  }

  int iHandle = 0;

  switch (size) {
  case AVATAR_SMALL:
    iHandle = SteamFriends()->GetSmallFriendAvatar(SteamUser()->GetSteamID());
    size = 32;
    break;

  case AVATAR_MEDIUM:
    iHandle = SteamFriends()->GetMediumFriendAvatar(SteamUser()->GetSteamID());
    size = 64;
    break;

  case AVATAR_LARGE:
    iHandle = SteamFriends()->GetLargeFriendAvatar(SteamUser()->GetSteamID());
    size = 184;
    break;

  default:
    return;
  }

  if (iHandle <= 0) {
    return;
  }

  // Has already loaded, simulate callback
  AvatarImageLoaded_t *avatarData = new AvatarImageLoaded_t;

  avatarData->m_steamID = SteamUser()->GetSteamID();
  avatarData->m_iImage = iHandle;
  avatarData->m_iWide = size;
  avatarData->m_iTall = size;

  _avatar_loaded(avatarData);

  delete avatarData;

  return;
}

// Signal that the avatar has been loaded
void GodotSteamFriends::_avatar_loaded(AvatarImageLoaded_t *avatarData) {
  if (avatarData->m_steamID != SteamUser()->GetSteamID()) {
    return;
  }

  int size = avatarData->m_iWide;
  // Get image buffer
  int buffSize = 4 * size * size;
  uint8 *iBuffer = new uint8[buffSize];
  bool success =
      SteamUtils()->GetImageRGBA(avatarData->m_iImage, iBuffer, buffSize);
  if (!success) {
    printf("[Steam] Failed to load image buffer from callback\n");
    return;
  }
  int rSize;
  if (size == 32) {
    rSize = AVATAR_SMALL;
  } else if (size == 64) {
    rSize = AVATAR_MEDIUM;
  } else if (size == 184) {
    rSize = AVATAR_LARGE;
  } else {
    printf("[Steam] Invalid avatar size from callback\n");
    return;
  }
  Image avatar = drawAvatar(size, iBuffer);
  call_deferred("emit_signal", "avatar_loaded", rSize, avatar);
}

// Draw the given avatar
Image GodotSteamFriends::drawAvatar(int iSize, uint8 *iBuffer) {
  // Apply buffer to Image
  Image avatar(iSize, iSize, false, Image::FORMAT_RGBA);
  for (int y = 0; y < iSize; y++) {
    for (int x = 0; x < iSize; x++) {
      int index = 4 * (x + y * iSize);

      float r = float(iBuffer[index]) / 255;
      float g = float(iBuffer[index + 1]) / 255;
      float b = float(iBuffer[index + 2]) / 255;
      float a = float(iBuffer[index + 3]) / 255;

      avatar.put_pixel(x, y, Color(r, g, b, a));
    }
  }

  return avatar;
}

void GodotSteamFriends::activateGameOverlay(const String &url) {
  if (!isSteamFriendsReady()) {
    return;
  }

  SteamFriends()->ActivateGameOverlay(url.utf8().get_data());
}

void GodotSteamFriends::activateGameOverlayToUser(const String &url,
                                                  int steamID) {
  if (!isSteamFriendsReady()) {
    return;
  }

  CSteamID overlayUserID =
      GodotSteamUtils::get_singleton()->createSteamID(steamID);
  SteamFriends()->ActivateGameOverlayToUser(url.utf8().get_data(),
                                            overlayUserID);
}

void GodotSteamFriends::activateGameOverlayToWebPage(const String &url) {
  if (!isSteamFriendsReady()) {
    return;
  }
  SteamFriends()->ActivateGameOverlayToWebPage(url.utf8().get_data());
}

void GodotSteamFriends::activateGameOverlayToStore(int app_id) {
  if (!isSteamFriendsReady()) {
    return;
  }
  SteamFriends()->ActivateGameOverlayToStore(AppId_t(app_id),
                                             EOverlayToStoreFlag(0));
}

Array GodotSteamFriends::getUserSteamGroups() {
  if (!isSteamFriendsReady()) {
    return Array();
  }
  int groupCount = SteamFriends()->GetClanCount();
  Array steamGroups;
  for (int index = 0; index < groupCount; index++) {
    Dictionary groups;
    CSteamID groupID = SteamFriends()->GetClanByIndex(index);
    String gName = SteamFriends()->GetClanName(groupID);
    String gTag = SteamFriends()->GetClanTag(groupID);
    groups["id"] = groupID.GetAccountID();
    groups["name"] = gName;
    groups["tag"] = gTag;
    steamGroups.append(groups);
  }
  return steamGroups;
}

Array GodotSteamFriends::getUserSteamFriends() {
  if (!isSteamFriendsReady()) {
    return Array();
  }
  int fCount = SteamFriends()->GetFriendCount(0x04);
  Array steamFriends;
  for (int index = 0; index < fCount; index++) {
    Dictionary friends;
    CSteamID friendID = SteamFriends()->GetFriendByIndex(index, 0x04);
    String fName = SteamFriends()->GetFriendPersonaName(friendID);
    int fStatus = SteamFriends()->GetFriendPersonaState(friendID);
    friends["id"] = friendID.GetAccountID();
    friends["name"] = fName;
    friends["status"] = fStatus;
    steamFriends.append(friends);
  }
  return steamFriends;
}

void GodotSteamFriends::activateGameOverlayInviteDialog(int steamID) {
  if (!isSteamFriendsReady()) {
    return;
  }
  CSteamID lobbyID = GodotSteamUtils::get_singleton()->createSteamID(steamID);
  SteamFriends()->ActivateGameOverlayInviteDialog(lobbyID);
}

void GodotSteamFriends::_bind_methods() {
  ObjectTypeDB::bind_method("getFriendCount",
                            &GodotSteamFriends::getFriendCount);
  ObjectTypeDB::bind_method("getPersonaName",
                            &GodotSteamFriends::getPersonaName);
  ObjectTypeDB::bind_method("getFriendPersonaName",
                            &GodotSteamFriends::getFriendPersonaName);
  ObjectTypeDB::bind_method(_MD("setGameInfo", "key", "value"),
                            &GodotSteamFriends::setGameInfo);
  ObjectTypeDB::bind_method(_MD("clearGameInfo"),
                            &GodotSteamFriends::clearGameInfo);
  ObjectTypeDB::bind_method(_MD("inviteFriend", "steam_id", "connect_string"),
                            &GodotSteamFriends::inviteFriend);
  ObjectTypeDB::bind_method(_MD("setPlayedWith", "steam_id"),
                            &GodotSteamFriends::setPlayedWith);
  ObjectTypeDB::bind_method("getRecentPlayers",
                            &GodotSteamFriends::getRecentPlayers);
  ObjectTypeDB::bind_method(_MD("getFriendAvatar", "size"),
                            &GodotSteamFriends::getFriendAvatar,
                            DEFVAL(AVATAR_MEDIUM));
  ObjectTypeDB::bind_method("getUserSteamGroups",
                            &GodotSteamFriends::getUserSteamGroups);
  ObjectTypeDB::bind_method("getUserSteamFriends",
                            &GodotSteamFriends::getUserSteamFriends);
  ObjectTypeDB::bind_method(_MD("activateGameOverlay", "type"),
                            &GodotSteamFriends::activateGameOverlay,
                            DEFVAL(""));
  ObjectTypeDB::bind_method(
      _MD("activateGameOverlayToUser", "type", "steam_id"),
      &GodotSteamFriends::activateGameOverlayToUser, DEFVAL(""));
  ObjectTypeDB::bind_method(_MD("activateGameOverlayToWebPage", "url"),
                            &GodotSteamFriends::activateGameOverlayToWebPage);
  ObjectTypeDB::bind_method(_MD("activateGameOverlayToStore", "appID"),
                            &GodotSteamFriends::activateGameOverlayToStore,
                            DEFVAL(0));
  ObjectTypeDB::bind_method(
      _MD("activateGameOverlayInviteDialog", "steam_id"),
      &GodotSteamFriends::activateGameOverlayInviteDialog);

  ADD_SIGNAL(MethodInfo("avatar_loaded", PropertyInfo(Variant::INT, "size"),
                        PropertyInfo(Variant::IMAGE, "avatar")));

  BIND_CONSTANT(AVATAR_SMALL);
  BIND_CONSTANT(AVATAR_MEDIUM);
  BIND_CONSTANT(AVATAR_LARGE);
}