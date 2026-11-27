#pragma once
#include <string>

using namespace std;

bool account_exists();
bool valid_password(const string& password);
void register_account(const string& password);
bool check_password(const string& password);