/* See xmpp.c file for copyright and license details. */

static LmHandlerResult cb_roster(LmMessageHandler *h, LmConnection *c,
                                 LmMessage *m, gpointer user_data);
static LmHandlerResult cb_version(LmMessageHandler *h, LmConnection *c,
                                  LmMessage *m, gpointer user_data);
static LmHandlerResult cb_time(LmMessageHandler *h, LmConnection *c,
                               LmMessage *m, gpointer user_data);
static LmHandlerResult cb_last(LmMessageHandler *h, LmConnection *c,
                               LmMessage *m, gpointer user_data);
static LmHandlerResult cb_vcard(LmMessageHandler *h, LmConnection *c,
                               LmMessage *m, gpointer user_data);

static struct IqRequestHandlers
{
  const gchar *xmlns;
  const gchar *querytag;
  LmHandleMessageFunction handler;
} iq_request_handlers[] = {
  {NS_ROSTER, "query", &cb_roster},
  {NS_VERSION,"query", &cb_version},
  {NS_TIME,   "query", &cb_time},
  {NS_LAST,   "query", &cb_last},
  {NS_VCARD,  "vCard", &cb_vcard},
  {NULL, NULL, NULL}
};

// Enum for vCard attributes
enum vcard_attr {
  vcard_home    = 1<<0,
  vcard_work    = 1<<1,
  vcard_postal  = 1<<2,
  vcard_voice   = 1<<3,
  vcard_fax     = 1<<4,
  vcard_cell    = 1<<5,
  vcard_inet    = 1<<6,
  vcard_pref    = 1<<7,
};

// xmlns has to be a namespace from iq_request_handlers[].xmlns
void xmpp_iq_request(const char *fulljid, const char *xmlns)
{
  LmMessage *iq;
  LmMessageNode *query;
  LmMessageHandler *handler;
  int i;

  iq = lm_message_new_with_sub_type(fulljid, LM_MESSAGE_TYPE_IQ,
                                    LM_MESSAGE_SUB_TYPE_GET);
  for (i = 0; strcmp(iq_request_handlers[i].xmlns, xmlns) != 0 ; ++i)
       ;
  query = lm_message_node_add_child(iq->node,
                                    iq_request_handlers[i].querytag,
                                    NULL);
  lm_message_node_set_attribute(query, "xmlns", xmlns);
  handler = lm_message_handler_new(iq_request_handlers[i].handler,
                                   NULL, FALSE);
  lm_connection_send_with_reply(lconnection, iq, handler, NULL);
  lm_message_handler_unref(handler);
  lm_message_unref(iq);
}

//  This callback is reached when mcabber receives the first roster update
// after the connection.
static LmHandlerResult cb_roster(LmMessageHandler *h, LmConnection *c,
                                 LmMessage *m, gpointer user_data)
{
  LmMessageNode *x;
  const char *ns;

  // Only execute the hook if the roster has been successfully retrieved
  if (lm_message_get_sub_type(m) != LM_MESSAGE_SUB_TYPE_RESULT)
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

  x = lm_message_node_find_child(m->node, "query");
  if (!x)
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

  ns = lm_message_node_get_attribute(x, "xmlns");
  if (ns && !strcmp(ns, NS_ROSTER))
    handle_iq_roster(NULL, c, m, user_data);

  // Post-login stuff
  hook_execute_internal("hook-post-connect");

  return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult cb_version(LmMessageHandler *h, LmConnection *c,
                                  LmMessage *m, gpointer user_data)
{
  LmMessageNode *ansqry;
  const char *p, *bjid;
  char *tmp;
  char *buf;

  ansqry = lm_message_node_get_child(m->node, "query");
  if (!ansqry) {
    scr_LogPrint(LPRINT_LOGNORM, "Invalid IQ:version result!");
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
  }

  // Display IQ result sender...
  p = lm_message_get_from(m);
  if (!p) {
    scr_LogPrint(LPRINT_LOGNORM, "Invalid IQ:version result (no sender name).");
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
  }
  bjid = p;

  buf = g_strdup_printf("Received IQ:version result from <%s>", bjid);
  scr_LogPrint(LPRINT_LOGNORM, "%s", buf);

  // bjid should now really be the "bare JID", let's strip the resource
  tmp = strchr(bjid, JID_RESOURCE_SEPARATOR);
  if (tmp) *tmp = '\0';

  scr_WriteIncomingMessage(bjid, buf, 0, HBB_PREFIX_INFO, 0);
  g_free(buf);

  // Get result data...
  p = lm_message_node_get_child_value(ansqry, "name");
  if (p) {
    buf = g_strdup_printf("Name:    %s", p);
    scr_WriteIncomingMessage(bjid, buf,
                             0, HBB_PREFIX_INFO | HBB_PREFIX_CONT, 0);
    g_free(buf);
  }
  p = lm_message_node_get_child_value(ansqry, "version");
  if (p) {
    buf = g_strdup_printf("Version: %s", p);
    scr_WriteIncomingMessage(bjid, buf,
                             0, HBB_PREFIX_INFO | HBB_PREFIX_CONT, 0);
    g_free(buf);
  }
  p = lm_message_node_get_child_value(ansqry, "os");
  if (p) {
    buf = g_strdup_printf("OS:      %s", p);
    scr_WriteIncomingMessage(bjid, buf,
                             0, HBB_PREFIX_INFO | HBB_PREFIX_CONT, 0);
    g_free(buf);
  }
  return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult cb_time(LmMessageHandler *h, LmConnection *c,
                               LmMessage *m, gpointer user_data)
{
  LmMessageNode *ansqry;
  const char *p, *bjid;
  char *tmp;
  char *buf;

  ansqry = lm_message_node_get_child(m->node, "query");
  if (!ansqry) {
    scr_LogPrint(LPRINT_LOGNORM, "Invalid IQ:time result!");
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
  }
  // Display IQ result sender...
  p = lm_message_get_from(m);
  if (!p) {
    scr_LogPrint(LPRINT_LOGNORM, "Invalid IQ:time result (no sender name).");
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
  }
  bjid = p;

  buf = g_strdup_printf("Received IQ:time result from <%s>", bjid);
  scr_LogPrint(LPRINT_LOGNORM, "%s", buf);

  // bjid should now really be the "bare JID", let's strip the resource
  tmp = strchr(bjid, JID_RESOURCE_SEPARATOR);
  if (tmp) *tmp = '\0';

  scr_WriteIncomingMessage(bjid, buf, 0, HBB_PREFIX_INFO, 0);
  g_free(buf);

  // Get result data...
  p = lm_message_node_get_child_value(ansqry, "utc");
  if (p) {
    buf = g_strdup_printf("UTC:  %s", p);
    scr_WriteIncomingMessage(bjid, buf,
                             0, HBB_PREFIX_INFO | HBB_PREFIX_CONT, 0);
    g_free(buf);
  }
  p = lm_message_node_get_child_value(ansqry, "tz");
  if (p) {
    buf = g_strdup_printf("TZ:   %s", p);
    scr_WriteIncomingMessage(bjid, buf,
                             0, HBB_PREFIX_INFO | HBB_PREFIX_CONT, 0);
    g_free(buf);
  }
  p = lm_message_node_get_child_value(ansqry, "display");
  if (p) {
    buf = g_strdup_printf("Time: %s", p);
    scr_WriteIncomingMessage(bjid, buf,
                             0, HBB_PREFIX_INFO | HBB_PREFIX_CONT, 0);
    g_free(buf);
  }
  return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult cb_last(LmMessageHandler *h, LmConnection *c,
                               LmMessage *m, gpointer user_data)
{
  LmMessageNode *ansqry;
  const char *p, *bjid;
  char *buf, *tmp;

  ansqry = lm_message_node_get_child(m->node, "query");
  if (!ansqry) {
    scr_LogPrint(LPRINT_LOGNORM, "Invalid IQ:last result!");
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
  }
  // Display IQ result sender...
  p = lm_message_get_from(m);
  if (!p) {
    scr_LogPrint(LPRINT_LOGNORM, "Invalid IQ:last result (no sender name).");
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
  }
  bjid = p;

  buf = g_strdup_printf("Received IQ:last result from <%s>", bjid);
  scr_LogPrint(LPRINT_LOGNORM, "%s", buf);

  // bjid should now really be the "bare JID", let's strip the resource
  tmp = strchr(bjid, JID_RESOURCE_SEPARATOR);
  if (tmp) *tmp = '\0';

  scr_WriteIncomingMessage(bjid, buf, 0, HBB_PREFIX_INFO, 0);
  g_free(buf);

  // Get result data...
  p = lm_message_node_get_attribute(ansqry, "seconds");
  if (p) {
    long int s;
    GString *sbuf;
    sbuf = g_string_new("Idle time: ");
    s = atol(p);
    // Days
    if (s > 86400L) {
      g_string_append_printf(sbuf, "%ldd ", s/86400L);
      s %= 86400L;
    }
    // hh:mm:ss
    g_string_append_printf(sbuf, "%02ld:", s/3600L);
    s %= 3600L;
    g_string_append_printf(sbuf, "%02ld:%02ld", s/60L, s%60L);
    scr_WriteIncomingMessage(bjid, sbuf->str,
                             0, HBB_PREFIX_INFO | HBB_PREFIX_CONT, 0);
    g_string_free(sbuf, TRUE);
  } else {
    scr_WriteIncomingMessage(bjid, "No idle time reported.",
                             0, HBB_PREFIX_INFO | HBB_PREFIX_CONT, 0);
  }
  p = lm_message_node_get_value(ansqry);
  if (p) {
    buf = g_strdup_printf("Status message: %s", p);
    scr_WriteIncomingMessage(bjid, buf, 0, HBB_PREFIX_INFO, 0);
    g_free(buf);
  }
  return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void display_vcard_item(const char *bjid, const char *label,
                               enum vcard_attr vcard_attrib, const char *text)
{
  char *buf;

  if (!text || !bjid || !label)
    return;

  buf = g_strdup_printf("%s: %s%s%s%s%s%s%s%s%s%s", label,
                        (vcard_attrib & vcard_home ? "[home]" : ""),
                        (vcard_attrib & vcard_work ? "[work]" : ""),
                        (vcard_attrib & vcard_postal ? "[postal]" : ""),
                        (vcard_attrib & vcard_voice ? "[voice]" : ""),
                        (vcard_attrib & vcard_fax  ? "[fax]"  : ""),
                        (vcard_attrib & vcard_cell ? "[cell]" : ""),
                        (vcard_attrib & vcard_inet ? "[inet]" : ""),
                        (vcard_attrib & vcard_pref ? "[pref]" : ""),
                        (vcard_attrib ? " " : ""),
                        text);
  scr_WriteIncomingMessage(bjid, buf, 0, HBB_PREFIX_INFO | HBB_PREFIX_CONT, 0);
  g_free(buf);
}

static void handle_vcard_node(const char *barejid, LmMessageNode *vcardnode)
{
  LmMessageNode *x;
  const char *p;

  for (x = vcardnode->children ; x; x = x->next) {
    const char *data;
    enum vcard_attr vcard_attrib = 0;

    p = x->name;
    data = lm_message_node_get_value(x);
    if (!p || !data)
      continue;

    if (!strcmp(p, "FN"))
      display_vcard_item(barejid, "Name", vcard_attrib, data);
    else if (!strcmp(p, "NICKNAME"))
      display_vcard_item(barejid, "Nickname", vcard_attrib, data);
    else if (!strcmp(p, "URL"))
      display_vcard_item(barejid, "URL", vcard_attrib, data);
    else if (!strcmp(p, "BDAY"))
      display_vcard_item(barejid, "Birthday", vcard_attrib, data);
    else if (!strcmp(p, "TZ"))
      display_vcard_item(barejid, "Timezone", vcard_attrib, data);
    else if (!strcmp(p, "TITLE"))
      display_vcard_item(barejid, "Title", vcard_attrib, data);
    else if (!strcmp(p, "ROLE"))
      display_vcard_item(barejid, "Role", vcard_attrib, data);
    else if (!strcmp(p, "DESC"))
      display_vcard_item(barejid, "Comment", vcard_attrib, data);
    else if (!strcmp(p, "N")) {
      data = lm_message_node_get_child_value(x, "FAMILY");
      display_vcard_item(barejid, "Family Name", vcard_attrib, data);
      data = lm_message_node_get_child_value(x, "GIVEN");
      display_vcard_item(barejid, "Given Name", vcard_attrib, data);
      data = lm_message_node_get_child_value(x, "MIDDLE");
      display_vcard_item(barejid, "Middle Name", vcard_attrib, data);
    } else if (!strcmp(p, "ORG")) {
      data = lm_message_node_get_child_value(x, "ORGNAME");
      display_vcard_item(barejid, "Organisation name", vcard_attrib, data);
      data = lm_message_node_get_child_value(x, "ORGUNIT");
      display_vcard_item(barejid, "Organisation unit", vcard_attrib, data);
    } else {
      // The HOME, WORK and PREF attributes are common to the remaining fields
      // (ADR, TEL & EMAIL)
      if (lm_message_node_get_child(x, "HOME"))
        vcard_attrib |= vcard_home;
      if (lm_message_node_get_child(x, "WORK"))
        vcard_attrib |= vcard_work;
      if (lm_message_node_get_child(x, "PREF"))
        vcard_attrib |= vcard_pref;
      if (!strcmp(p, "ADR")) {          // Address
        if (lm_message_node_get_child(x, "POSTAL"))
          vcard_attrib |= vcard_postal;
        data = lm_message_node_get_child_value(x, "EXTADD");
        display_vcard_item(barejid, "Addr (ext)", vcard_attrib, data);
        data = lm_message_node_get_child_value(x, "STREET");
        display_vcard_item(barejid, "Street", vcard_attrib, data);
        data = lm_message_node_get_child_value(x, "LOCALITY");
        display_vcard_item(barejid, "Locality", vcard_attrib, data);
        data = lm_message_node_get_child_value(x, "REGION");
        display_vcard_item(barejid, "Region", vcard_attrib, data);
        data = lm_message_node_get_child_value(x, "PCODE");
        display_vcard_item(barejid, "Postal code", vcard_attrib, data);
        data = lm_message_node_get_child_value(x, "CTRY");
        display_vcard_item(barejid, "Country", vcard_attrib, data);
      } else if (!strcmp(p, "TEL")) {   // Telephone
        data = lm_message_node_get_child_value(x, "NUMBER");
        if (data) {
          if (lm_message_node_get_child(x, "VOICE"))
            vcard_attrib |= vcard_voice;
          if (lm_message_node_get_child(x, "FAX"))
            vcard_attrib |= vcard_fax;
          if (lm_message_node_get_child(x, "CELL"))
            vcard_attrib |= vcard_cell;
          display_vcard_item(barejid, "Phone", vcard_attrib, data);
        }
      } else if (!strcmp(p, "EMAIL")) { // Email
        if (lm_message_node_get_child(x, "INTERNET"))
          vcard_attrib |= vcard_inet;
        data = lm_message_node_get_child_value(x, "USERID");
        display_vcard_item(barejid, "Email", vcard_attrib, data);
      }
    }
  }
}

static LmHandlerResult cb_vcard(LmMessageHandler *h, LmConnection *c,
                               LmMessage *m, gpointer user_data)
{
  LmMessageNode *ansqry;
  const char *p, *bjid;
  char *buf, *tmp;

  // Display IQ result sender...
  p = lm_message_get_from(m);
  if (!p) {
    scr_LogPrint(LPRINT_LOGNORM, "Invalid IQ:vCard result (no sender name).");
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
  }
  bjid = p;

  buf = g_strdup_printf("Received IQ:vCard result from <%s>", bjid);
  scr_LogPrint(LPRINT_LOGNORM, "%s", buf);

  // Get the vCard node
  ansqry = lm_message_node_get_child(m->node, "vCard");
  if (!ansqry) {
    scr_LogPrint(LPRINT_LOGNORM, "Empty IQ:vCard result!");
    g_free(buf);
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
  }

  // bjid should really be the "bare JID", let's strip the resource
  tmp = strchr(bjid, JID_RESOURCE_SEPARATOR);
  if (tmp) *tmp = '\0';

  scr_WriteIncomingMessage(bjid, buf, 0, HBB_PREFIX_INFO, 0);
  g_free(buf);

  // Get result data...
  handle_vcard_node(bjid, ansqry);
  return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void storage_bookmarks_parse_conference(LmMessageNode *node)
{
  const char *fjid, *name, *autojoin;
  const char *pstatus, *awhois;
  char *bjid;
  GSList *room_elt;

  fjid = lm_message_node_get_attribute(node, "jid");
  if (!fjid)
    return;
  name = lm_message_node_get_attribute(node, "name");
  autojoin = lm_message_node_get_attribute(node, "autojoin");
  awhois = lm_message_node_get_attribute(node, "autowhois");
  pstatus = lm_message_node_get_child_value(node, "print_status");

  bjid = jidtodisp(fjid); // Bare jid

  // Make sure this is a room (it can be a conversion user->room)
  room_elt = roster_find(bjid, jidsearch, 0);
  if (!room_elt) {
    room_elt = roster_add_user(bjid, name, NULL, ROSTER_TYPE_ROOM,
                               sub_none, -1);
  } else {
    buddy_settype(room_elt->data, ROSTER_TYPE_ROOM);
    /*
    // If the name is available, should we use it?
    // I don't think so, it would be confusing because this item is already
    // in the roster.
    if (name)
      buddy_setname(room_elt->data, name);
    */
  }

  // Set the print_status and auto_whois values
  if (pstatus) {
    enum room_printstatus i;
    for (i = status_none; i <= status_all; i++)
      if (!strcasecmp(pstatus, strprintstatus[i]))
        break;
    if (i <= status_all)
      buddy_setprintstatus(room_elt->data, i);
  }
  if (awhois) {
    enum room_autowhois i = autowhois_default;
    if (!strcmp(awhois, "1"))
      i = autowhois_on;
    else if (!strcmp(awhois, "0"))
      i = autowhois_off;
    if (i != autowhois_default)
      buddy_setautowhois(room_elt->data, i);
  }

  // Is autojoin set?
  // If it is, we'll look up for more information (nick? password?) and
  // try to join the room.
  if (autojoin && !strcmp(autojoin, "1")) {
    const char *nick, *passwd;
    char *tmpnick = NULL;
    nick = lm_message_node_get_child_value(node, "nick");
    passwd = lm_message_node_get_child_value(node, "password");
    if (!nick || !*nick)
      nick = tmpnick = default_muc_nickname(NULL);
    // Let's join now
    scr_LogPrint(LPRINT_LOGNORM, "Auto-join bookmark <%s>", bjid);
    xmpp_room_join(bjid, nick, passwd);
    g_free(tmpnick);
  }
  g_free(bjid);
}

static LmHandlerResult cb_storage_bookmarks(LmMessageHandler *h,
                                            LmConnection *c,
                                            LmMessage *m, gpointer user_data)
{
  LmMessageNode *x, *ansqry;
  char *p;

  if (lm_message_get_sub_type(m) == LM_MESSAGE_SUB_TYPE_ERROR) {
    // No server support, or no bookmarks?
    p = m->node->children->name;
    if (p && !strcmp(p, "item-not-found")) {
      // item-no-found means the server has Private Storage, but it's
      // currently empty.
      if (bookmarks)
        lm_message_node_unref(bookmarks);
      bookmarks = lm_message_node_new("storage", "storage:bookmarks");
      // We return 0 so that the IQ error message be
      // not displayed, as it isn't a real error.
      return LM_HANDLER_RESULT_REMOVE_MESSAGE;
    }
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS; // Unhandled error
  }

  ansqry = lm_message_node_get_child(m->node, "query");
  ansqry = lm_message_node_get_child(ansqry, "storage");
  if (!ansqry) {
    scr_LogPrint(LPRINT_LOG, "Invalid IQ:private result! (storage:bookmarks)");
    return 0;
  }

  // Walk through the storage tags
  for (x = ansqry->children ; x; x = x->next) {
    // If the current node is a conference item, parse it and update the roster
    if (x->name && !strcmp(x->name, "conference"))
      storage_bookmarks_parse_conference(x);
  }
  // "Copy" the bookmarks node
  if (bookmarks)
    lm_message_node_unref(bookmarks);
  lm_message_node_deep_ref(ansqry);
  bookmarks = ansqry;
  return 0;
}


static LmHandlerResult cb_storage_rosternotes(LmMessageHandler *h,
                                              LmConnection *c,
                                              LmMessage *m, gpointer user_data)
{
  LmMessageNode *ansqry;

  if (lm_message_get_sub_type(m) == LM_MESSAGE_SUB_TYPE_ERROR) {
    const char *p;
    // No server support, or no roster notes?
    p = m->node->children->name;
    if (p && !strcmp(p, "item-not-found")) {
      // item-no-found means the server has Private Storage, but it's
      // currently empty.
      if (rosternotes)
        lm_message_node_unref(rosternotes);
      rosternotes = lm_message_node_new("storage", "storage:rosternotes");
      // We return 0 so that the IQ error message be
      // not displayed, as it isn't a real error.
      return LM_HANDLER_RESULT_REMOVE_MESSAGE;
    }
    return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS; // Unhandled error
  }

  ansqry = lm_message_node_get_child(m->node, "query");
  ansqry = lm_message_node_get_child(ansqry, "storage");
  if (!ansqry) {
    scr_LogPrint(LPRINT_LOG, "Invalid IQ:private result! "
                 "(storage:rosternotes)");
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
  }
  // Copy the rosternotes node
  if (rosternotes)
    lm_message_node_unref(rosternotes);
  lm_message_node_deep_ref(ansqry);
  rosternotes = ansqry;
  return 0;
}


static struct IqRequestStorageHandlers
{
  const gchar *storagens;
  LmHandleMessageFunction handler;
} iq_request_storage_handlers[] = {
  {"storage:rosternotes", &cb_storage_rosternotes},
  {"storage:bookmarks", &cb_storage_bookmarks},
  {NULL, NULL}
};

void xmpp_request_storage(const gchar *storage)
{
  LmMessage *iq;
  LmMessageNode *query;
  LmMessageHandler *handler;
  int i;

  iq = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_IQ,
                                    LM_MESSAGE_SUB_TYPE_GET);
  query = lm_message_node_add_child(iq->node, "query", NULL);
  lm_message_node_set_attribute(query, "xmlns", NS_PRIVATE);
  lm_message_node_set_attribute(lm_message_node_add_child
                                (query, "storage", NULL),
                                "xmlns", storage);

  for (i = 0;
       strcmp(iq_request_storage_handlers[i].storagens, storage) != 0;
       ++i) ;

  handler = lm_message_handler_new(iq_request_storage_handlers[i].handler,
                                   NULL, FALSE);
  lm_connection_send_with_reply(lconnection, iq, handler, NULL);
  lm_message_handler_unref(handler);
  lm_message_unref(iq);
}
