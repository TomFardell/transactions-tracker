# Transactions Tracker

This is a website hosted on a web server written in C. Currently I'm just hosting this locally.

## Modified HTML

In order to retrieve data from the program, I have added some additional syntax to the HTML files. This is inspired by [Handlebars.js](https://handlebarsjs.com/). It is a little bit clunkier, but seems to work pretty well for what I need. 

### Mappings

Variables in the program need to be mapped into identifiers that can be used in the HTML. There are three types of mapping:
- List mappings map a linked list from the program into a 'list element' in the HTML. These list elements cannot be accessed directly, but there are commands (see below) that take these as arguments
- Item mappings map a single variable in the program to an identifier in the HTML. Depending on their type, these can either be output directly on the page or used to access members
- Member mappings relate to members of some struct in the code. Member mappings allow us to display/use members of items or lists 

There are display types set in these mappings to determine how data is displayed on the page. For example, the `DATE` display type would mean data is formatted like a date when put on the page. Similarly, the `CURRENCY` display type would cause a `£` sign to be appended to the field on the page. Note that there are restrictions for which internal types can have which display types (i.e. it wouldn't make sense to have a date displaying as a currency).

### Syntax

This additional syntax works with tags that look like `<!--{{...}}-->`. Note, Handlebars.js uses the simpler `{{...}}` tags, but using this syntax would cause my HTML LSP to see errors.

To reference a literal in the HTML, we encase it within this tag. For example, to put the value of `my_var` on the page (after mapping it in in the code), simply write `<!--{{my_var}}-->`. For example, to put it in a `<p>` tag, we could write `<p><!--{{my_var}}--></p>`.

For example, suppose we have the following data in the program (with the appopriate mappings):

```c
// Pseudocode
U32Vec2 my_point = {2, 3};  // NUMBER display type (for .x and .y)
F32 my_balance = 32.74;     // CURRENCY display type
```

Then we can insert these values into the parsed HTML:

```html
<p>My point is (<!--{{my_point.x}}-->, <!--{{my_point.y}}-->) and my balance is <!--{{my_balance}}--></p>
```

After parsing, we see:

```html
<p>My point is (2, 3) and my balance is £32.74</p>
```

### Commands

There are a number of special commands that can be used to dynamically alter the HTML.

---

#### #each

The `#each` command allows looping over a list mapping. It takes one argument: a list to iterate over. It is accompanied by a closing `/each` tag, which signals the end of the HTML region to be repeated per element in the list. In each iteration, the special identifer `this` references that element of the linked list.

As an example, suppose we have a list mapping to a linked list of points:

```cpp
// Pseudocode
LinkedList<U32Vec2> points = {{2, 3}, {4, 5}, {6, 7}};
```

Then we could use the `#each` command in this modified HTML to print all the points:

```html
<!--{{#each points}}-->
<p>(<!--{{this.x}}-->, <!--{{this.y}}-->)</p>
<!--{{/each}}-->
```

This becomes the following after parsing:

```html
<p>(2, 3)</p>
<p>(4, 5)</p>
<p>(6, 7)</p>
```

Importantly, we can get members from the `this` special item. It behaves just as any other item mapping.

---

#### #sum

The `#sum` command allows for summing over a linked list. It expects a single argument: the list (and any members) to sum over. The `#sum` tag is simply replaced by the summed value.

Suppose we have a couple of lists we want to sum:

```cpp
// Pseudocode
LinkedList<F32> balances = {12.50, 12.50, 24.99};
LinkedList<U32Vec2> points = {{2, 3}, {4, 5}, {6, 7}};
```

Then we could use the `#sum` command to sum up the balances, as well as the components of the points:

```html
<p>Total of balances: <!--{{#sum balances}}--></p>
<p>Total x of points: <!--{{#sum points.x}}--></p>
<p>Total y of points: <!--{{#sum points.y}}--></p>
```

After parsing, this becomes:

```html
<p>Total of balances: £49.99</p>
<p>Total x of points: 12</p>
<p>Total y of points: 15</p>
```

Note we can only sum over certain data types. For example, it wouldn't make sense to sum dates. Also note the syntax for summing over members of each item in a list.

---

#### #if

The `#if` command allows for conditional existence of a block of HTML. It expects 3 arguments: two identifiers or literals separated by a comparison operator, i.e. `#if my_var <= 2`. A closing `\if` tag is used to encompass the region of HTML that should only be included in the parsed result should the condition evaluate to true.

Suppose we have two variables:

```c
// Pseudocode
U32 my_num1 = 10;
U32 my_num2 = 14;
```

Then we could use `#if` tags to only show particular paragraphs on the site:

```html
<!--{{#if my_num1 != 5}}-->
<p>The first number isn't 5</p>
<!--{{/if}}-->
<!--{{#if 10 > my_num2}}-->
<p>The second number is less than 10</p>
<!--{{/if}}-->
<!--{{#if my_num1 < my_num2}}-->
<p>The second number is larger than the first number</p>
<!--{{/if}}-->
<!--{{#if -1 == 1}}-->
<p>This will never show</p>
<!--{{/if}}-->
```

After parsing, this becomes:

```html
<p>The first number isn't 5</p>
<p>The second number is larger than the first number</p>
```

Where this becomes particularly useful is when we use HTTP POSTs to alter a variable. This would allow for dynamic altering of the HTML based on responses to e.g. buttons. 

It is worth noting that more complex conditions, including `else` statements are not currently supported; the arguments must take the form `<identifier/literal> <operator> <identifier/literal>`. The supported comparison operators are `==`, `<`, `<=`, `>`, `>=` and `!=`.

---

#### #iter

The `#iter` command takes a non-negative integer item as its only argument and is used to repeat a region of HTML a certain number of times. We can retrieve which iteration we are in within the repeated block using `this`, which runs from 0 (inclusive) to the value of the argument (exclusive). We use a closing `/iter` tag to encompass the block of HTML that will be repeated.

Suppose we have the following value in the code:
```c
// Pseudocode
U32 num_iterations = 4;
```

Then we could use the `#iter` command to repeat some paragraphs:

```html
<!--{{#iter num_iterations}}-->
<p>This is paragraph <!--{{this}}--></p>
<!--{{/iter}}-->
```

After parsing, we get the following:

```html
<p>This is paragraph 0</p>
<p>This is paragraph 1</p>
<p>This is paragraph 2</p>
<p>This is paragraph 3</p>
```

---

## HTTP POST Method Handling

The other half of making the website dynamic is to use particular HTML forms to send instructions back to the code.

I've not thought about how this will work yet, but there will probably be some commands to add items to lists, as well as to update values in the code.
