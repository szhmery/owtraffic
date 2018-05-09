/*
 * NetworkNamespace.h
 *
 *  Created on: May 1, 2018
 *      Author: changqwa
 */

#ifndef NETWORKNAMESPACE_H_
#define NETWORKNAMESPACE_H_

int already_in_namespace();
int set_netns(const char *ns);
int deescalate();

#endif /* NETWORKNAMESPACE_H_ */
